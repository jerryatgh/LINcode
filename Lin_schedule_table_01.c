

/*
Description: LIN master Schedule
Input parameters: none (use my_schedule structure)
Output parameters: '1' if a frame has been transmitted, '0' otherwise.
*/

#define	unsigned char	uc8

uc8 LIN_Schedule_Tick(void)
{
	if(LinTxCtr>0)
	{
		LinTxCtr--;
	}
	lin_status = lin_get_status(&cmd_lin);
	if(lin_status==LIN_ACTION_ONGOING)
	{
		//do nothing.
	}
	else if(lin_status==LIN_ACTION_COMPLETED)
	{
		if(cmd_lin.cmd==LIN_CMD_M_RX)
		{
			DataReceived_Notification((msg_lin.id & 0x0f), id_to_dlc(msg_lin.id));	//id_to_dlc()...
			Lin_clear_status();
		}
		else
		{
			#ifdef	_SLEEP_TIMOUT_DETECTION
			//check if sleep command has been sent.
			if(lin_sleep_message_request)
			{
				lin_sleep_message_request = 0;
				lin_master_in_sleep_mode = 1;
				//LIN sleep message has been successfully sent.
				//go to sleep and wait for a wake up (from slave or master command)
				Lin_cmd_gotosleep();
			}
			#endif
		}
	}
	else if((lin_status==LIN_AVAILABLE) && (LinTxCtr==0))
	{
		Start_Lin_Master();
	}
	else
	{
		//error occured?
		#ifdef CAN_LIN_GATEWAY_ENABLE
		if(lin_status & 0x80)
			SendCanErrorFrame = 1;	//send a can error frame.
			LinErrorMessage = lin_status;
		#endif
		#ifdef _COUNTING_ERRORS_ENABLE
		update_error_counter(lin_status);
		#endif	
		
		if(LinTxCtr==0)
		{
			Start_Lin_Master();
		}
	}
	
	return 0;
}



#ifdef	_COUNTING_ERRORS_ENABLE
/*
Description: update error counters
Input parameters: lin status
Output parameters: none
Issue: 循环中使用了else if语句，如果status同时
	出现了bit error & no responding，改程序只会
	检测到bit error。
*/
void update_error_counter(uc8 status)
{
	if((status & LIN_ERROR_BIT)==LIN_ERROR_BIT)
	{
		//bit error
		countBEER++;
	}
	else if((status & LIN_ERROR_CSUM)==LIN_ERROR_CSUM)
	{
		//checksum error
		countCEER++;
	}
	else if((status & LIN_ERROR_PARITY)==LIN_ERROR_PARITY)
	{
		//LIN ID parity error
		countPEER++;
	}
	else if((status & LIN_ERROR_SYNCHRO)==LIN_ERROR_SYNCHRO)
	{
		//synchro byte error
		countSEER++;
	}
	else if((status & LIN_ERROR_NO_RESPONDING)==LIN_ERROR_NO_RESPONDING)
	{
		//slave not responding error
		countREER++;
	}
}
#endif


/*
Description: start transmitting a new LIN frame
Input parameters: none (use my_schedule structure and frame_index)
Output parameters: none (use my_schedule structure)
Issue: if(0)下面的“右边花括号”应该在#endif上面。
*/
void Start_Lin_Master(void)
{
	t_schedule*	pt_schedule;
	uc8 i;
	
	pt_schedule = &my_schedule;
	Lin_clear_status();
	if(Frame_index >= (pt_schedule->number_of_frame-1))
	{
		Frame_index = 0;
	}
	else
	{
		Frame_index++;
	}
	
	#ifdef _SLEEP_TIMOUT_DETECTION
	if(lin_sleep_message_request)
	{
		//check if sleep message tx request is pending.
		for(i=0;i<8;i++)
		{
			tab_data[i] = 0;
		}
		
		msg_lin.id = ComputeIdField(0x0C,8);		//...
		cmd_lin.cmd = LIN_CMD_M_TX;		//sent a sleep Frame. ???
		cmd_lin.pt_lin_msg = &msg_lin;
		msg_lin.pt_data = &tab_data[0];
		LinTxCtr = 2000;		//???
		lin_cmd(&cmd_lin);
	}
	else if(lin_master_in_sleep_mode)
	{
		LinTxCtr = 100;		//???
	}
	#else
	if(0)
	{
	#endif
	}
	else if( pt_schedule->frame_message[Frame_index].frame_type == STANDART_LIN_FRAME_TYPE )
	{
		//data request: notify frame_id & frame_size.
		DataRequest_Notification( pt_schedule->frame_message[Frame_index].frame_id, pt_schedule->frame_message[Frame_index].frame_size );
		//msg id compute: according to frame_id & frame_size
		msg_lin.id = ComputeIdField( pt_schedule->frame_message[Frame_index].frame_id, pt_schedule->frame_message[Frame_index].frame_size );
		//sent a new LIN frame.
		cmd_lin.cmd = LIN_CMD_M_TX;
		//get address of LIN message. ???
		cmd_lin.pt_lin_msg = &msg_lin;
		//get the address of LIN data. ???
		msg_lin.pt_data = &tab_data[0];
		//???
		LinTxCtr = pt_schedule->frame_message[Frame_index].frame_delay;
		//???
		lin_cmd(&cmd_lin);
	}
	else if( pt_schedule->frame_message[Frame_index].frame_type == REMOTE_LIN_FRAME_TYPE )
	{
		//response in frame.
		msg_lin.id = ComputeIdField( pt_schedule->frame_message[Frame_index].frame_id, pt_schedule->frame_message[Frame_index].frame_size );
		//sent a new LIN frame. RX??? not TX???
		cmd_lin.cmd = LIN_CMD_M_RX;
		//get address of LIN message. ???
		cmd_lin.pt_lin_msg = &msg_lin;
		//get the address of LIN data. ???
		msg_lin.pt_data = &tab_data[0];
		//???
		LinTxCtr = pt_schedule->frame_message[Frame_index].frame_delay;
		//???
		lin_cmd(&cmd_lin);
	}


/*
Description: update DLC of ID to be transferred.
Input parameters: LIN_ID without DLC; data length of characters to be transferred.
Output parameters: LIN_ID with DLC.
Issue: 
*/
uc8 ComputeIdField(uc8 lin_id_wo_dlc, uc8 dlc)
{
	uc8 tep_id;
	tmp_id = lin_id_wo_dlc & 0xCF;	//clear bits4-5 of DLC.
	
	if( dlc==2 )
	{
		//2 bytes to be transferred.
		tmp_id |=0x10;
	}
	else if( dlc==4 )
	{
		//4 bytes to be transferred.
		tmp_id |=0x20;
	}
	else if( dlc==8 )
	{
		//8 bytes to be transferred.
		tmp_id |=0x30;
	}
	//Jerry: do not need else{} ???
	
	return tep_id;
}











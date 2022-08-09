// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_MESSAGES_H_
#define REMOTING_HOST_CHROMOTING_MESSAGES_H_

#include "ipc/ipc_message_start.h"

#endif  // REMOTING_HOST_CHROMOTING_MESSAGES_H_

// Multiply-included message file, no traditional include guard.
#include "ipc/ipc_message_macros.h"

#define IPC_MESSAGE_START ChromotingMsgStart

//-----------------------------------------------------------------------------
// Chromoting messages sent from the daemon.

// Requests the receiving process to crash producing a crash dump. The daemon
// sends this message when a fatal error has been detected indicating that
// the receiving process misbehaves. The daemon passes the location of the code
// that detected the error.
IPC_MESSAGE_CONTROL(ChromotingDaemonMsg_Crash,
                    std::string /* function_name */,
                    std::string /* file_name */,
                    int /* line_number */)

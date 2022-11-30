// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

#define IPC_MESSAGE_START TestMsgStart

IPC_MESSAGE_CONTROL0(TestMsg_Hello)
IPC_MESSAGE_CONTROL0(TestMsg_Quit)
IPC_MESSAGE_CONTROL1(TestMsg_Ping, std::string)
IPC_SYNC_MESSAGE_CONTROL1_1(TestMsg_SyncPing, std::string, std::string)

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

// Singly-included section for enums and custom IPC traits.
#ifndef IPC_IPC_CHANNEL_PROXY_UNITTEST_MESSAGES_H_
#define IPC_IPC_CHANNEL_PROXY_UNITTEST_MESSAGES_H_

class BadType {
 public:
  BadType() {}
};

namespace IPC {

template <>
struct ParamTraits<BadType> {
  static void Write(base::Pickle* m, const BadType& p) {}
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   BadType* r) {
    return false;
  }
  static void Log(const BadType& p, std::string* l) {}
};

}

#endif  // IPC_IPC_CHANNEL_PROXY_UNITTEST_MESSAGES_H_

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START TestMsgStart
IPC_MESSAGE_CONTROL0(TestMsg_Bounce)
IPC_MESSAGE_CONTROL0(TestMsg_SendBadMessage)
IPC_MESSAGE_CONTROL1(TestMsg_BadMessage, BadType)

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START AutomationMsgStart
IPC_MESSAGE_CONTROL0(AutomationMsg_Bounce)

#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START WorkerMsgStart
IPC_MESSAGE_CONTROL0(WorkerMsg_Bounce)
IPC_MESSAGE_CONTROL0(WorkerMsg_Quit)

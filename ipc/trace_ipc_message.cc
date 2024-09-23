// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/trace_ipc_message.h"

#include <stdint.h>

#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_legacy_ipc.pbzero.h"

namespace IPC {

using perfetto::protos::pbzero::ChromeLegacyIpc;

void WriteIpcMessageIdAsProtozero(uint32_t message_id,
                                  ChromeLegacyIpc* legacy_ipc) {
  ChromeLegacyIpc::MessageClass message_class =
      ChromeLegacyIpc::CLASS_UNSPECIFIED;
  switch (IPC_MESSAGE_ID_CLASS(message_id)) {
    case AutomationMsgStart:
      message_class = ChromeLegacyIpc::CLASS_AUTOMATION;
      break;
    case TestMsgStart:
      message_class = ChromeLegacyIpc::CLASS_TEST;
      break;
    case WorkerMsgStart:
      message_class = ChromeLegacyIpc::CLASS_WORKER;
      break;
    case NaClMsgStart:
      message_class = ChromeLegacyIpc::CLASS_NACL;
      break;
    case PpapiMsgStart:
      message_class = ChromeLegacyIpc::CLASS_PPAPI;
      break;
    case NaClHostMsgStart:
      message_class = ChromeLegacyIpc::CLASS_NACL_HOST;
      break;
  }
  legacy_ipc->set_message_class(message_class);
  legacy_ipc->set_message_line(IPC_MESSAGE_ID_LINE(message_id));
}

}  // namespace IPC

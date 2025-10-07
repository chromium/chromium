// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/trace_ipc_message.h"

#include <stdint.h>

#include "ipc/ipc_message_macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_legacy_ipc.pbzero.h"

namespace IPC {

using perfetto::protos::pbzero::ChromeLegacyIpc;

void WriteIpcMessageIdAsProtozero(uint32_t message_id,
                                  ChromeLegacyIpc* legacy_ipc) {
  legacy_ipc->set_message_class(ChromeLegacyIpc::CLASS_TEST);
  legacy_ipc->set_message_line(IPC_MESSAGE_ID_LINE(message_id));
}

}  // namespace IPC

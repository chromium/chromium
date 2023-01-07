// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_TRACE_IPC_MESSAGE_H_
#define IPC_TRACE_IPC_MESSAGE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_legacy_ipc.pbzero.h"

// When tracing is enabled, emits a trace event with the given category and
// event name and typed arguments for the message's type (message class and line
// number).
#define TRACE_IPC_MESSAGE_SEND(category, name, msg)                          \
  TRACE_EVENT(category, name, [msg](perfetto::EventContext ctx) {            \
    IPC::WriteIpcMessageIdAsProtozero(msg->type(),                           \
                                      ctx.event()->set_chrome_legacy_ipc()); \
  });

namespace IPC {

// Converts |message_id| into its message class and line number parts and writes
// them to the protozero message |ChromeLegacyIpc| for trace events.
void COMPONENT_EXPORT(IPC)
    WriteIpcMessageIdAsProtozero(uint32_t message_id,
                                 perfetto::protos::pbzero::ChromeLegacyIpc*);

}  // namespace IPC

#endif  // IPC_TRACE_IPC_MESSAGE_H_

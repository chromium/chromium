// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/send_message_helper.h"

#include <cstring>

#include "base/macros.h"
#include "base/trace_event/typed_macros.h"

namespace mojo {
namespace internal {

void SendMessage(MessageReceiver& receiver, Message& message) {
  TRACE_EVENT_INSTANT("toplevel.flow", "Send mojo message",
                      perfetto::Flow::Global(message.GetTraceId()));
  ignore_result(receiver.Accept(&message));
}

void SendMessage(MessageReceiverWithResponder& receiver,
                 Message& message,
                 std::unique_ptr<MessageReceiver> responder) {
  TRACE_EVENT_INSTANT("toplevel.flow", "Send mojo message",
                      perfetto::Flow::Global(message.GetTraceId()));
  ignore_result(receiver.AcceptWithResponder(&message, std::move(responder)));
}

}  // namespace internal
}  // namespace mojo

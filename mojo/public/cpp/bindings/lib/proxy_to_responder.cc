// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/proxy_to_responder.h"

#include <utility>

#include "base/check.h"
#include "mojo/public/cpp/bindings/lib/send_message_helper.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

ProxyToResponder::ProxyToResponder(const Message& message,
                                   std::unique_ptr<ResponderThunk>&& responder)
    : request_id_(message.request_id()),
      trace_nonce_(message.header()->trace_nonce),
      // Responses mirror the sync flag of the request. They are never sent
      // with kFlagNoInterrupt or kFlagIsUrgent.
      response_flags_(
          Message::kFlagIsResponse |
          (message.has_flag(Message::kFlagIsSync) ? Message::kFlagIsSync : 0)),
      responder_(std::move(responder)) {}

ProxyToResponder::~ProxyToResponder() {
  // If the Callback was dropped then deleting the responder will close
  // the pipe so the calling application knows to stop waiting for a reply.
  responder_.reset();
}

void ProxyToResponder::SendResponse(Message& message) {
  DCHECK(responder_);
  message.set_request_id(request_id_);
  message.set_trace_nonce(trace_nonce_);
  SendMojoMessage(*responder_, message);
  // SendMojoMessage() fails silently if the responder connection is closed,
  // or if the message is malformed.
  //
  // TODO(darin): If Accept() returns false due to a malformed message, that
  // may be good reason to close the connection. However, we don't have a
  // way to do that from here. We should add a way.
  responder_ = nullptr;
}

}  // namespace internal
}  // namespace mojo

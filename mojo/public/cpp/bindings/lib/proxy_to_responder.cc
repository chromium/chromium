// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/proxy_to_responder.h"

#include <utility>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "mojo/public/cpp/bindings/lib/send_message_helper.h"
#include "mojo/public/cpp/bindings/message.h"

#if DCHECK_IS_ON()
#include "base/task/thread_pool/thread_pool_instance.h"
#endif

namespace mojo {
namespace internal {

ProxyToResponder::ProxyToResponder(
    const Message& message,
    std::unique_ptr<ResponderThunk>&& responder
#if DCHECK_IS_ON()
    ,
    OnIsConnectedCompleteFunc on_is_connected_complete
#endif
    )
    : request_id_(message.request_id()),
      trace_nonce_(message.header()->trace_nonce),
      // Responses mirror the sync flag of the request. They are never sent
      // with kFlagNoInterrupt or kFlagIsUrgent.
      response_flags_(
          Message::kFlagIsResponse |
          (message.has_flag(Message::kFlagIsSync) ? Message::kFlagIsSync : 0)),
      responder_(std::move(responder))
#if DCHECK_IS_ON()
      ,
      on_is_connected_complete_(on_is_connected_complete)
#endif
{
}

ProxyToResponder::~ProxyToResponder() {
#if DCHECK_IS_ON()
  if (responder_ && on_is_connected_complete_) {
    // If we're being destroyed without being run, we want to ensure the
    // binding endpoint has been closed. This checks for that asynchronously.
    // We pass a bound generated callback to handle the response so that any
    // resulting DCHECK stack will have useful interface type information.
    // Instantiate a ScopedFizzleBlockShutdownTasks to allow this request to
    // fizzle if this happens after shutdown and the endpoint is bound to a
    // BLOCK_SHUTDOWN sequence.
    base::ThreadPoolInstance::ScopedFizzleBlockShutdownTasks fizzler;
    responder_->IsConnectedAsync(base::BindOnce(on_is_connected_complete_));
  }
#endif
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

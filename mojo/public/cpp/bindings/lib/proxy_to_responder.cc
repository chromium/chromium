// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/proxy_to_responder.h"

#include <cstring>

#include "mojo/public/cpp/bindings/message.h"

namespace mojo {
namespace internal {

ProxyToResponder::ProxyToResponder(
    const Message& message,
    std::unique_ptr<MessageReceiverWithStatus> responder)
    : request_id_(message.request_id()),
      trace_nonce_(message.header()->trace_nonce),
      is_sync_(message.has_flag(Message::kFlagIsSync)),
      responder_(std::move(responder)) {}

ProxyToResponder::~ProxyToResponder() {
  // If the Callback was dropped then deleting the responder will close
  // the pipe so the calling application knows to stop waiting for a reply.
  responder_.reset();
}

}  // namespace internal
}  // namespace mojo

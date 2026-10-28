// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/lib/responder_thunk.h"

namespace mojo {

class Message;

namespace internal {

// Base class for the generated *_ProxyToResponder classes, which hold the state
// needed to reply to a single request. Instances are single-use: each is owned
// by a reply callback (a base::OnceCallback), and SendResponse() clears
// `responder_` once the response is sent.
//
// Destroying an instance without sending a response (i.e. dropping the reply
// callback) is an error unless the interface pipe has already been closed
// (generated subclasses DCHECK this). In that case, destroying `responder_`
// raises a connection error so that the caller stops waiting for a reply.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ProxyToResponder {
 public:
  ProxyToResponder(const Message& message,
                   std::unique_ptr<ResponderThunk>&& responder);
  ~ProxyToResponder();

  ProxyToResponder(const ProxyToResponder&) = delete;
  ProxyToResponder& operator=(const ProxyToResponder&) = delete;

 protected:
  // Sends `message` as the response, and resets `responder_`. Must be called at
  // most once. Not inlined in order to reduce the size of generated code.
  void SendResponse(Message& message);

  uint64_t request_id_;
  uint32_t trace_nonce_;
  // The flags to use for the response message. Computed at runtime (rather
  // than by the bindings generator) since they depend on whether the request
  // was sync.
  uint32_t response_flags_;
  std::unique_ptr<ResponderThunk> responder_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

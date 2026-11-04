// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "mojo/public/cpp/bindings/lib/responder_thunk.h"

namespace mojo {

class Message;

namespace internal {

// Holds the state needed to reply to a single request. Instances are
// single-use: each is owned by a reply callback (a base::OnceCallback), and
// SendResponse() clears `responder_` once the response is sent.
//
// Destroying an instance without sending a response (i.e. dropping the reply
// callback) is an error unless the interface pipe has already been closed
// (checked asynchronously in DCHECK builds). In that case, destroying
// `responder_` raises a connection error so that the caller stops waiting for
// a reply.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ProxyToResponder {
 public:
#if DCHECK_IS_ON()
  using OnIsConnectedCompleteFunc = void (*)(bool);
#endif
  ~ProxyToResponder();

  ProxyToResponder(const ProxyToResponder&) = delete;
  ProxyToResponder& operator=(const ProxyToResponder&) = delete;

  uint32_t response_flags() const { return response_flags_; }

  // Sends `message` as the response, and resets `responder_`. Must be called at
  // most once. Not inlined in order to reduce the size of generated code.
  void SendResponse(Message& message);

  template <typename... Args>
  NOINLINE static base::OnceCallback<void(Args...)> CreateCallback(
      void (*run_func)(ProxyToResponder*, Args...),
      const Message& message,
      std::unique_ptr<ResponderThunk>&& responder
#if DCHECK_IS_ON()
      ,
      OnIsConnectedCompleteFunc on_is_connected_complete
#endif
  ) {
    auto* proxy = new ProxyToResponder(message, std::move(responder)
#if DCHECK_IS_ON()
                                                    ,
                                       on_is_connected_complete
#endif
    );
    return base::BindOnce(run_func, base::Owned(proxy));
  }

 private:
  ProxyToResponder(const Message& message,
                   std::unique_ptr<ResponderThunk>&& responder
#if DCHECK_IS_ON()
                   ,
                   OnIsConnectedCompleteFunc on_is_connected_complete
#endif
  );

  uint64_t request_id_;
  uint32_t trace_nonce_;
  // The flags to use for the response message. Computed at runtime (rather
  // than by the bindings generator) since they depend on whether the request
  // was sync.
  uint32_t response_flags_;
  std::unique_ptr<ResponderThunk> responder_;
#if DCHECK_IS_ON()
  OnIsConnectedCompleteFunc on_is_connected_complete_ = nullptr;
#endif
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

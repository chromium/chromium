// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"

namespace mojo {

class Message;
class MessageReceiverWithStatus;

namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) ProxyToResponder {
 public:
  ProxyToResponder(const Message& message,
                   std::unique_ptr<MessageReceiverWithStatus> responder);
  ~ProxyToResponder();

  ProxyToResponder(const ProxyToResponder&) = delete;
  ProxyToResponder& operator=(const ProxyToResponder&) = delete;

 protected:
  uint64_t request_id_;
  uint32_t trace_nonce_;
  bool is_sync_;
  std::unique_ptr<MessageReceiverWithStatus> responder_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_PROXY_TO_RESPONDER_H_

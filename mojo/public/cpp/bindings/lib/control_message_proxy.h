// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_PROXY_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"

namespace mojo {

class InterfaceEndpointClient;

namespace internal {

// Proxy for request messages defined in interface_control_messages.mojom.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ControlMessageProxy {
 public:
  // Doesn't take ownership of |owner|. It must outlive this object.
  explicit ControlMessageProxy(InterfaceEndpointClient* owner);
  ~ControlMessageProxy();

  void QueryVersion(base::OnceCallback<void(uint32_t)> callback);
  void RequireVersion(uint32_t version);

  void FlushForTesting();
  void FlushAsyncForTesting(base::OnceClosure callback);

  void EnableIdleTracking(base::TimeDelta timeout);
  void SendMessageAck();
  void NotifyIdle();

  void OnConnectionError();

 private:
  void RunFlushForTestingClosure();

  // Not owned.
  InterfaceEndpointClient* const owner_;
  bool encountered_error_ = false;

  base::OnceClosure pending_flush_callback_;

  DISALLOW_COPY_AND_ASSIGN(ControlMessageProxy);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_CONTROL_MESSAGE_PROXY_H_

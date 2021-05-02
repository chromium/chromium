// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <algorithm>  // For |std::swap()|.
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {
namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) AssociatedInterfacePtrStateBase {
 public:
  AssociatedInterfacePtrStateBase();
  ~AssociatedInterfacePtrStateBase();

  uint32_t version() const { return version_; }

  void QueryVersion(base::OnceCallback<void(uint32_t)> callback);
  void RequireVersion(uint32_t version);
  void FlushForTesting();
  void CloseWithReason(uint32_t custom_reason, const std::string& description);

  bool is_bound() const { return !!endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  AssociatedGroup* associated_group() {
    return endpoint_client_ ? endpoint_client_->associated_group() : nullptr;
  }

  void ForwardMessage(Message message) { endpoint_client_->Accept(&message); }

  void ForwardMessageWithResponder(Message message,
                                   std::unique_ptr<MessageReceiver> responder) {
    endpoint_client_->AcceptWithResponder(&message, std::move(responder));
  }

  void force_outgoing_messages_async(bool force) {
    DCHECK(endpoint_client_);
    endpoint_client_->force_outgoing_messages_async(force);
  }

 protected:
  void Swap(AssociatedInterfacePtrStateBase* other);
  void Bind(ScopedInterfaceEndpointHandle handle,
            uint32_t version,
            std::unique_ptr<MessageReceiver> validator,
            scoped_refptr<base::SequencedTaskRunner> runner,
            const char* interface_name);
  ScopedInterfaceEndpointHandle PassHandle();

  InterfaceEndpointClient* endpoint_client() { return endpoint_client_.get(); }

 private:
  void OnQueryVersion(base::OnceCallback<void(uint32_t)> callback,
                      uint32_t version);

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;
  uint32_t version_ = 0;
};

template <typename Interface>
class AssociatedInterfacePtrState : public AssociatedInterfacePtrStateBase {
 public:
  using Proxy = typename Interface::Proxy_;

  AssociatedInterfacePtrState() {}
  ~AssociatedInterfacePtrState() = default;

  Proxy* instance() {
    // This will be null if the object is not bound.
    return proxy_.get();
  }

  void Swap(AssociatedInterfacePtrState* other) {
    AssociatedInterfacePtrStateBase::Swap(other);
    std::swap(other->proxy_, proxy_);
  }

  void Bind(AssociatedInterfacePtrInfo<Interface> info,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!proxy_);
    AssociatedInterfacePtrStateBase::Bind(
        info.PassHandle(), info.version(),
        std::make_unique<typename Interface::ResponseValidator_>(),
        std::move(runner), Interface::Name_);
    proxy_.reset(new Proxy(endpoint_client()));
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  AssociatedInterfacePtrInfo<Interface> PassInterface() {
    AssociatedInterfacePtrInfo<Interface> info(PassHandle(), version());
    proxy_.reset();
    return info;
  }

 private:
  std::unique_ptr<Proxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(AssociatedInterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_ASSOCIATED_INTERFACE_PTR_STATE_H_

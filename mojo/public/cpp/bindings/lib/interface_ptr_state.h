// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <algorithm>  // For |std::swap()|.
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {
namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InterfacePtrStateBase {
 public:
  InterfacePtrStateBase();
  ~InterfacePtrStateBase();

  MessagePipeHandle handle() const {
    return router_ ? router_->handle() : handle_.get();
  }

  uint32_t version() const { return version_; }

  bool is_bound() const { return handle_.is_valid() || endpoint_client_; }

  bool encountered_error() const {
    return endpoint_client_ ? endpoint_client_->encountered_error() : false;
  }

  bool HasAssociatedInterfaces() const {
    return router_ ? router_->HasAssociatedEndpoints() : false;
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const {
    return endpoint_client_ && endpoint_client_->has_pending_responders();
  }

  void force_outgoing_messages_async(bool force) {
    DCHECK(endpoint_client_);
    endpoint_client_->force_outgoing_messages_async(force);
  }

#if DCHECK_IS_ON()
  void SetNextCallLocation(const base::Location& location) {
    endpoint_client_->SetNextCallLocation(location);
  }
#endif

 protected:
  InterfaceEndpointClient* endpoint_client() const {
    return endpoint_client_.get();
  }
  MultiplexRouter* router() const { return router_.get(); }

  void QueryVersion(base::OnceCallback<void(uint32_t)> callback);
  void RequireVersion(uint32_t version);
  void Swap(InterfacePtrStateBase* other);
  void Bind(PendingRemoteState* remote_state,
            scoped_refptr<base::SequencedTaskRunner> task_runner);

  ScopedMessagePipeHandle PassMessagePipe() {
    endpoint_client_.reset();
    return router_ ? router_->PassMessagePipe() : std::move(handle_);
  }

  bool InitializeEndpointClient(
      bool passes_associated_kinds,
      bool has_sync_methods,
      std::unique_ptr<MessageReceiver> payload_validator,
      const char* interface_name);

 private:
  void OnQueryVersion(base::OnceCallback<void(uint32_t)> callback,
                      uint32_t version);

  scoped_refptr<MultiplexRouter> router_;

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;

  // |router_| (as well as other members above) is not initialized until
  // read/write with the message pipe handle is needed. |handle_| is valid
  // between the Bind() call and the initialization of |router_|.
  ScopedMessagePipeHandle handle_;
  scoped_refptr<base::SequencedTaskRunner> runner_;

  uint32_t version_ = 0;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtrStateBase);
};

template <typename Interface>
class InterfacePtrState : public InterfacePtrStateBase {
 public:
  using Proxy = typename Interface::Proxy_;

  InterfacePtrState() = default;
  ~InterfacePtrState() = default;

  Proxy* instance() {
    ConfigureProxyIfNecessary();

    // This will be null if the object is not bound.
    return proxy_.get();
  }

  void SetNextCallLocation(const base::Location& location) {
#if DCHECK_IS_ON()
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::SetNextCallLocation(location);
#endif
  }

  void QueryVersion(base::OnceCallback<void(uint32_t)> callback) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::QueryVersion(std::move(callback));
  }

  void RequireVersion(uint32_t version) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::RequireVersion(version);
  }

  void FlushForTesting() {
    ConfigureProxyIfNecessary();
    endpoint_client()->FlushForTesting();
  }

  void FlushAsyncForTesting(base::OnceClosure callback) {
    ConfigureProxyIfNecessary();
    endpoint_client()->FlushAsyncForTesting(std::move(callback));
  }

  void CloseWithReason(uint32_t custom_reason, const std::string& description) {
    ConfigureProxyIfNecessary();
    endpoint_client()->CloseWithReason(custom_reason, description);
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->proxy_, proxy_);
    InterfacePtrStateBase::Swap(other);
  }

  void Bind(PendingRemoteState* remote_state,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!proxy_);
    InterfacePtrStateBase::Bind(remote_state, std::move(runner));
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  InterfacePtrInfo<Interface> PassInterface() {
    proxy_.reset();
    return InterfacePtrInfo<Interface>(PassMessagePipe(), version());
  }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(endpoint_client());
    endpoint_client()->set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    ConfigureProxyIfNecessary();

    DCHECK(endpoint_client());
    endpoint_client()->set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  void set_idle_handler(base::TimeDelta timeout,
                        base::RepeatingClosure handler) {
    ConfigureProxyIfNecessary();
    DCHECK(endpoint_client());
    endpoint_client()->SetIdleHandler(timeout, std::move(handler));
  }

  unsigned int GetNumUnackedMessagesForTesting() const {
    return endpoint_client()->GetNumUnackedMessagesForTesting();
  }

  AssociatedGroup* associated_group() {
    ConfigureProxyIfNecessary();
    return endpoint_client()->associated_group();
  }

  void EnableTestingMode() {
    ConfigureProxyIfNecessary();
    router()->EnableTestingMode();
  }

  void ForwardMessage(Message message) {
    ConfigureProxyIfNecessary();
    endpoint_client()->Accept(&message);
  }

  void ForwardMessageWithResponder(Message message,
                                   std::unique_ptr<MessageReceiver> responder) {
    ConfigureProxyIfNecessary();
    endpoint_client()->AcceptWithResponder(&message, std::move(responder));
  }

  void RaiseError() {
    ConfigureProxyIfNecessary();
    endpoint_client()->RaiseError();
  }

 private:
  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      DCHECK(router());
      DCHECK(endpoint_client());
      return;
    }

    if (InitializeEndpointClient(
            Interface::PassesAssociatedKinds_, Interface::HasSyncMethods_,
            std::make_unique<typename Interface::ResponseValidator_>(),
            Interface::Name_)) {
      router()->SetMasterInterfaceName(Interface::Name_);
      proxy_ = std::make_unique<Proxy>(endpoint_client());
    }
  }

  std::unique_ptr<Proxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(InterfacePtrState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

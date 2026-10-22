// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"
#include "mojo/public/cpp/bindings/lib/sync_method_traits.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_metadata_helpers.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

class AssociatedGroup;
class InterfaceEndpointClient;

namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InterfacePtrStateBase {
 public:
  InterfacePtrStateBase();

  InterfacePtrStateBase(const InterfacePtrStateBase&) = delete;
  InterfacePtrStateBase& operator=(const InterfacePtrStateBase&) = delete;

  ~InterfacePtrStateBase();

  MessagePipeHandle handle() const {
    return router_ ? router_->handle() : handle_.get();
  }

  uint32_t version() const { return version_; }

  bool is_bound() const { return handle_.is_valid() || endpoint_client_; }

  bool encountered_error() const;

  bool HasAssociatedInterfaces() const {
    return router_ ? router_->HasAssociatedEndpoints() : false;
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const;

  scoped_refptr<ThreadSafeProxy> CreateThreadSafeProxy(
      scoped_refptr<ThreadSafeProxy::Target> target,
      const base::Location& location);

#if DCHECK_IS_ON()
  void SetNextCallLocation(const base::Location& location);
#endif

  unsigned int GetNumUnackedMessagesForTesting() const;

 protected:
  InterfaceEndpointClient* endpoint_client() const {
    return endpoint_client_.get();
  }
  MultiplexRouter* router() const { return router_.get(); }

  void QueryVersion(base::OnceCallback<void(uint32_t)> callback);
  void RequireVersion(uint32_t version);
  void PauseReceiverUntilFlushCompletes(PendingFlush flush);
  void FlushAsync(AsyncFlusher flusher);
  void FlushForTesting();
  void FlushAsyncForTesting(base::OnceClosure callback);
  void CloseWithReason(uint32_t custom_reason, std::string_view description);
  void SetFilter(std::unique_ptr<MessageFilter> filter);
  void set_connection_error_handler(base::OnceClosure error_handler);
  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler);
  void set_idle_handler(base::TimeDelta timeout,
                        base::RepeatingClosure handler);
  AssociatedGroup* associated_group();
  void RaiseError();
  void Swap(InterfacePtrStateBase* other);
  void Bind(PendingRemoteState* remote_state,
            scoped_refptr<base::SequencedTaskRunner> task_runner);
  PendingRemoteState Unbind();

  ScopedMessagePipeHandle PassMessagePipe();

  MessageReceiverWithResponder* InitializeEndpointClient(
      bool passes_associated_kinds,
      bool has_sync_methods,
      bool has_uninterruptable_methods,
      std::unique_ptr<MessageReceiver> payload_validator,
      const char* interface_name,
      MessageToMethodInfoCallback method_info_callback,
      MessageToMethodNameCallback method_name_callback);

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
};

template <typename Interface>
class InterfacePtrState : public InterfacePtrStateBase {
 public:
  using Proxy = typename Interface::Proxy_;

  InterfacePtrState() = default;

  InterfacePtrState(const InterfacePtrState&) = delete;
  InterfacePtrState& operator=(const InterfacePtrState&) = delete;

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

  void PauseReceiverUntilFlushCompletes(PendingFlush flush) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::PauseReceiverUntilFlushCompletes(std::move(flush));
  }

  void FlushAsync(AsyncFlusher flusher) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::FlushAsync(std::move(flusher));
  }

  void FlushForTesting() {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::FlushForTesting();
  }

  void FlushAsyncForTesting(base::OnceClosure callback) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::FlushAsyncForTesting(std::move(callback));
  }

  void CloseWithReason(uint32_t custom_reason, std::string_view description) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::CloseWithReason(custom_reason, description);
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->proxy_, proxy_);
    InterfacePtrStateBase::Swap(other);
  }

  void SetFilter(std::unique_ptr<MessageFilter> filter) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::SetFilter(std::move(filter));
  }

  void Bind(PendingRemoteState* remote_state,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!proxy_);
    InterfacePtrStateBase::Bind(remote_state, std::move(runner));
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  PendingRemoteState Unbind() {
    proxy_.reset();
    return InterfacePtrStateBase::Unbind();
  }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::set_connection_error_handler(
        std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  void set_idle_handler(base::TimeDelta timeout,
                        base::RepeatingClosure handler) {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::set_idle_handler(timeout, std::move(handler));
  }

  AssociatedGroup* associated_group() {
    ConfigureProxyIfNecessary();
    return InterfacePtrStateBase::associated_group();
  }

  void EnableTestingMode() {
    ConfigureProxyIfNecessary();
    router()->EnableTestingMode();
  }

  void RaiseError() {
    ConfigureProxyIfNecessary();
    InterfacePtrStateBase::RaiseError();
  }

  InterfaceEndpointClient* endpoint_client_for_test() {
    return endpoint_client();
  }

 private:
  void ConfigureProxyIfNecessary() {
    // The proxy has been configured.
    if (proxy_) {
      DCHECK(router());
      DCHECK(endpoint_client());
      return;
    }

    if (MessageReceiverWithResponder* endpoint = InitializeEndpointClient(
            Interface::PassesAssociatedKinds_,
            !SyncMethodTraits<Interface>::GetOrdinals().empty(),
            Interface::HasUninterruptableMethods_,
            std::make_unique<typename Interface::ResponseValidator_>(),
            Interface::Name_, Interface::MessageToMethodInfo_,
            Interface::MessageToMethodName_)) {
      proxy_ = std::make_unique<Proxy>(endpoint);
    }
  }

  std::unique_ptr<Proxy> proxy_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

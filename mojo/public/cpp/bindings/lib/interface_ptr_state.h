// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
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

struct PendingReceiverState;

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InterfacePtrStateBase {
 public:
  InterfacePtrStateBase();

  InterfacePtrStateBase(const InterfacePtrStateBase&) = delete;
  InterfacePtrStateBase& operator=(const InterfacePtrStateBase&) = delete;

  ~InterfacePtrStateBase();

  MessagePipeHandle handle() const {
    return router_ ? router_->handle() : MessagePipeHandle();
  }

  uint32_t version() const { return version_; }

  bool is_bound() const { return !!endpoint_client_; }

  bool encountered_error() const;

  bool HasAssociatedInterfaces() const {
    return router_ ? router_->HasAssociatedEndpoints() : false;
  }

  // Returns true if bound and awaiting a response to a message.
  bool has_pending_callbacks() const;

  scoped_refptr<ThreadSafeProxy> CreateThreadSafeProxy(
      scoped_refptr<ThreadSafeProxy::Target> target,
      const base::Location& location);

  unsigned int GetNumUnackedMessagesForTesting() const;

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
  void EnableTestingMode();
  void RaiseError();

 protected:
  InterfaceEndpointClient* endpoint_client() const {
    return endpoint_client_.get();
  }

  void Swap(InterfacePtrStateBase* other);
  // Returns a null responder if `remote_state->pipe` is invalid.
  MessageReceiverWithResponder* Bind(
      PendingRemoteState* remote_state,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      bool passes_associated_kinds,
      bool has_sync_methods,
      bool has_uninterruptable_methods,
      MessageValidatorFunc payload_validator,
      const char* interface_name,
      MessageToMethodInfoCallback method_info_callback,
      MessageToMethodNameCallback method_name_callback);
  MessageReceiverWithResponder* BindNewPipe(
      PendingReceiverState* receiver_state,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      bool passes_associated_kinds,
      bool has_sync_methods,
      bool has_uninterruptable_methods,
      MessageValidatorFunc payload_validator,
      const char* interface_name,
      MessageToMethodInfoCallback method_info_callback,
      MessageToMethodNameCallback method_name_callback);
  PendingRemoteState Unbind();

  ScopedMessagePipeHandle PassMessagePipe();

 private:
  void OnQueryVersion(base::OnceCallback<void(uint32_t)> callback,
                      uint32_t version);

  scoped_refptr<MultiplexRouter> router_;

  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;

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
    // This will be null if the object is not bound.
    return proxy_.get();
  }

  void Swap(InterfacePtrState* other) {
    using std::swap;
    swap(other->proxy_, proxy_);
    InterfacePtrStateBase::Swap(other);
  }

  void Bind(PendingRemoteState* remote_state,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!proxy_);
    // `Bind()` returns null if `remote_state->pipe` is invalid (e.g. when
    // `Remote::Bind()` is called with a null `PendingRemote`).
    auto* endpoint = InterfacePtrStateBase::Bind(
        remote_state, std::move(runner), Interface::PassesAssociatedKinds_,
        !SyncMethodTraits<Interface>::GetOrdinals().empty(),
        Interface::HasUninterruptableMethods_, GetPayloadValidator(),
        Interface::Name_, Interface::MessageToMethodInfo_,
        Interface::MessageToMethodName_);
    if (endpoint) {
      proxy_ = std::make_unique<Proxy>(endpoint);
    }
  }

  void BindNewPipe(PendingReceiverState* receiver_state,
                   scoped_refptr<base::SequencedTaskRunner> runner) {
    DCHECK(!proxy_);
    MessageReceiverWithResponder* endpoint = InterfacePtrStateBase::BindNewPipe(
        receiver_state, std::move(runner), Interface::PassesAssociatedKinds_,
        !SyncMethodTraits<Interface>::GetOrdinals().empty(),
        Interface::HasUninterruptableMethods_, GetPayloadValidator(),
        Interface::Name_, Interface::MessageToMethodInfo_,
        Interface::MessageToMethodName_);
    proxy_ = std::make_unique<Proxy>(endpoint);
  }

  // After this method is called, the object is in an invalid state and
  // shouldn't be reused.
  PendingRemoteState Unbind() {
    proxy_.reset();
    return InterfacePtrStateBase::Unbind();
  }

  InterfaceEndpointClient* endpoint_client_for_test() {
    return endpoint_client();
  }

 private:
  static constexpr MessageValidatorFunc GetPayloadValidator() {
    if constexpr (std::is_same_v<typename Interface::ResponseValidator_,
                                 PassThroughFilter>) {
      return nullptr;
    } else {
      return &Interface::ResponseValidator_::Accept;
    }
  }

  std::unique_ptr<Proxy> proxy_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_PTR_STATE_H_

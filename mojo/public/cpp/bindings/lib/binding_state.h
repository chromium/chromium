// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

namespace internal {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) BindingStateBase {
 public:
  BindingStateBase();
  ~BindingStateBase();

  void SetFilter(std::unique_ptr<MessageFilter> filter);

  bool HasAssociatedInterfaces() const;

  void PauseIncomingMethodCallProcessing();
  void ResumeIncomingMethodCallProcessing();

  bool WaitForIncomingMethodCall(
      MojoDeadline deadline = MOJO_DEADLINE_INDEFINITE);

  void Close();
  void CloseWithReason(uint32_t custom_reason, const std::string& description);

  void RaiseError() { endpoint_client_->RaiseError(); }

  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_handler(std::move(error_handler));
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK(is_bound());
    endpoint_client_->set_connection_error_with_reason_handler(
        std::move(error_handler));
  }

  bool is_bound() const { return !!router_; }

  MessagePipeHandle handle() const {
    DCHECK(is_bound());
    return router_->handle();
  }

  ReportBadMessageCallback GetBadMessageCallback();

  void FlushForTesting();

  void EnableBatchDispatch();

  void EnableTestingMode();

  scoped_refptr<internal::MultiplexRouter> RouterForTesting();

 protected:
  void BindInternal(PendingReceiverState* receiver_state,
                    scoped_refptr<base::SequencedTaskRunner> runner,
                    const char* interface_name,
                    std::unique_ptr<MessageReceiver> request_validator,
                    bool passes_associated_kinds,
                    bool has_sync_methods,
                    MessageReceiverWithResponderStatus* stub,
                    uint32_t interface_version);

  scoped_refptr<internal::MultiplexRouter> router_;
  std::unique_ptr<InterfaceEndpointClient> endpoint_client_;

  base::WeakPtrFactory<BindingStateBase> weak_ptr_factory_{this};
};

template <typename Interface, typename ImplRefTraits>
class BindingState : public BindingStateBase {
 public:
  using ImplPointerType = typename ImplRefTraits::PointerType;

  explicit BindingState(ImplPointerType impl) {
    stub_.set_sink(std::move(impl));
  }

  ~BindingState() { Close(); }

  void Bind(PendingReceiverState* receiver_state,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    BindingStateBase::BindInternal(
        std::move(receiver_state), runner, Interface::Name_,
        std::make_unique<typename Interface::RequestValidator_>(),
        Interface::PassesAssociatedKinds_, Interface::HasSyncMethods_, &stub_,
        Interface::Version_);
  }

  InterfaceRequest<Interface> Unbind() {
    weak_ptr_factory_.InvalidateWeakPtrs();
    endpoint_client_.reset();
    InterfaceRequest<Interface> request(router_->PassMessagePipe());
    router_ = nullptr;
    return request;
  }

  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }
  ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    Interface* old_impl = impl();
    stub_.set_sink(std::move(new_impl));
    return old_impl;
  }

 private:
  typename Interface::template Stub_<ImplRefTraits> stub_;

  DISALLOW_COPY_AND_ASSIGN(BindingState);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_

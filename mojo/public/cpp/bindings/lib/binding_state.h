// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"
#include "mojo/public/cpp/bindings/lib/sync_method_traits.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/pending_flush.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

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

  bool WaitForIncomingMethodCall();

  void PauseRemoteCallbacksUntilFlushCompletes(PendingFlush flush);
  void FlushAsync(AsyncFlusher flusher);

  void Close();
  void CloseWithReason(uint32_t custom_reason, std::string_view description);

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
                    base::span<const uint32_t> sync_method_ordinals,
                    MessageReceiverWithResponderStatus* stub,
                    uint32_t interface_version,
                    MessageToMethodInfoCallback method_info_callback,
                    MessageToMethodNameCallback method_name_callback);

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

  BindingState(const BindingState&) = delete;
  BindingState& operator=(const BindingState&) = delete;

  ~BindingState() { Close(); }

  void Bind(PendingReceiverState* receiver_state,
            scoped_refptr<base::SequencedTaskRunner> runner) {
    BindingStateBase::BindInternal(
        std::move(receiver_state), runner, Interface::Name_,
        std::make_unique<typename Interface::RequestValidator_>(),
        Interface::PassesAssociatedKinds_,
        SyncMethodTraits<Interface>::GetOrdinals(), &stub_, Interface::Version_,
        Interface::MessageToMethodInfo_, Interface::MessageToMethodName_);
  }

  PendingReceiver<Interface> Unbind() {
    weak_ptr_factory_.InvalidateWeakPtrs();
    endpoint_client_.reset();
    PendingReceiver<Interface> request(router_->PassMessagePipe());
    router_ = nullptr;
    return request;
  }

  Interface* impl() { return ImplRefTraits::GetRawPointer(&stub_.sink()); }
  ImplPointerType SwapImplForTesting(ImplPointerType new_impl) {
    return std::exchange(stub_.sink(), std::move(new_impl));
  }

 private:
  typename Interface::template Stub_<ImplRefTraits> stub_;
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_BINDING_STATE_H_

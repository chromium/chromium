// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/binding_state.h"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"
#include "mojo/public/cpp/bindings/mojo_buildflags.h"

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
#include "mojo/public/cpp/bindings/lib/test_random_mojo_delays.h"
#endif

namespace mojo {
namespace internal {

BindingStateBase::BindingStateBase() = default;

BindingStateBase::~BindingStateBase() = default;

void BindingStateBase::SetFilter(std::unique_ptr<MessageFilter> filter) {
  DCHECK(endpoint_client_);
  endpoint_client_->SetFilter(std::move(filter));
}

bool BindingStateBase::HasAssociatedInterfaces() const {
  return router_ ? router_->HasAssociatedEndpoints() : false;
}

void BindingStateBase::PauseIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->PauseIncomingMethodCallProcessing();
}

void BindingStateBase::ResumeIncomingMethodCallProcessing() {
  DCHECK(router_);
  router_->ResumeIncomingMethodCallProcessing();
}

bool BindingStateBase::WaitForIncomingMethodCall() {
  DCHECK(router_);
  return router_->WaitForIncomingMessage();
}

void BindingStateBase::PauseRemoteCallbacksUntilFlushCompletes(
    PendingFlush flush) {
  router_->PausePeerUntilFlushCompletes(std::move(flush));
}

void BindingStateBase::FlushAsync(AsyncFlusher flusher) {
  router_->FlushAsync(std::move(flusher));
}

void BindingStateBase::Close() {
  if (!router_)
    return;

  weak_ptr_factory_.InvalidateWeakPtrs();

  endpoint_client_.reset();
  router_->CloseMessagePipe();
  router_ = nullptr;
}

void BindingStateBase::CloseWithReason(uint32_t custom_reason,
                                       std::string_view description) {
  if (endpoint_client_)
    endpoint_client_->CloseWithReason(custom_reason, description);

  Close();
}

ReportBadMessageCallback BindingStateBase::GetBadMessageCallback() {
  return base::BindOnce(
      [](ReportBadMessageCallback inner_callback,
         base::WeakPtr<BindingStateBase> binding, std::string_view error) {
        std::move(inner_callback).Run(error);
        if (binding)
          binding->Close();
      },
      mojo::GetBadMessageCallback(), weak_ptr_factory_.GetWeakPtr());
}

void BindingStateBase::FlushForTesting() {
  endpoint_client_->FlushForTesting();
}

void BindingStateBase::EnableBatchDispatch() {
  DCHECK(is_bound());
  router_->EnableBatchDispatch();
}

void BindingStateBase::EnableTestingMode() {
  DCHECK(is_bound());
  router_->EnableTestingMode();
}

scoped_refptr<internal::MultiplexRouter> BindingStateBase::RouterForTesting() {
  return router_;
}

void BindingStateBase::BindInternal(
    PendingReceiverState* receiver_state,
    scoped_refptr<base::SequencedTaskRunner> runner,
    const char* interface_name,
    std::unique_ptr<MessageReceiver> request_validator,
    bool passes_associated_kinds,
    base::span<const uint32_t> sync_method_ordinals,
    MessageReceiverWithResponderStatus* stub,
    uint32_t interface_version,
    MessageToMethodInfoCallback method_info_callback,
    MessageToMethodNameCallback method_name_callback) {
  DCHECK(!is_bound()) << "Attempting to bind interface that is already bound: "
                      << interface_name;

  auto sequenced_runner =
      GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(runner));

  MultiplexRouter::Config config =
      passes_associated_kinds
          ? MultiplexRouter::MULTI_INTERFACE
          : (!sync_method_ordinals.empty()
                 ? MultiplexRouter::SINGLE_INTERFACE_WITH_SYNC_METHODS
                 : MultiplexRouter::SINGLE_INTERFACE);
  router_ = MultiplexRouter::CreateAndStartReceiving(
      std::move(receiver_state->pipe), config, false, sequenced_runner,
      interface_name);
  router_->SetConnectionGroup(std::move(receiver_state->connection_group));

  endpoint_client_ = std::make_unique<InterfaceEndpointClient>(
      router_->CreateLocalEndpointHandle(kPrimaryInterfaceId), stub,
      std::move(request_validator), sync_method_ordinals,
      std::move(sequenced_runner), interface_version, interface_name,
      method_info_callback, method_name_callback);
  endpoint_client_->SetIdleTrackingEnabledCallback(
      base::BindOnce(&MultiplexRouter::SetConnectionGroup, router_));

#if BUILDFLAG(MOJO_RANDOM_DELAYS_ENABLED)
  MakeBindingRandomlyPaused(base::SequencedTaskRunner::GetCurrentDefault(),
                            weak_ptr_factory_.GetWeakPtr());
#endif
}

}  // namespace internal
}  // namespace mojo

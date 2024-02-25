// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_receiver.h"

#include <memory>
#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/multiplex_router.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

namespace internal {

AssociatedReceiverBase::AssociatedReceiverBase() = default;

void AssociatedReceiverBase::SetFilter(std::unique_ptr<MessageFilter> filter) {
  DCHECK(endpoint_client_);
  endpoint_client_->SetFilter(std::move(filter));
}

void AssociatedReceiverBase::reset() {
  endpoint_client_.reset();
}

void AssociatedReceiverBase::ResetWithReason(uint32_t custom_reason,
                                             std::string_view description) {
  // TODO(dcheng): This should unconditionally assert that there is an endpoint
  // client.
  if (endpoint_client_)
    endpoint_client_->CloseWithReason(custom_reason, description);
  reset();
}

void AssociatedReceiverBase::set_disconnect_handler(
    base::OnceClosure error_handler) {
  DCHECK(is_bound());
  endpoint_client_->set_connection_error_handler(std::move(error_handler));
}

void AssociatedReceiverBase::set_disconnect_with_reason_handler(
    ConnectionErrorWithReasonCallback error_handler) {
  DCHECK(is_bound());
  endpoint_client_->set_connection_error_with_reason_handler(
      std::move(error_handler));
}

void AssociatedReceiverBase::reset_on_disconnect() {
  DCHECK(is_bound());
  set_disconnect_handler(
      base::BindOnce(&AssociatedReceiverBase::reset, base::Unretained(this)));
}

void AssociatedReceiverBase::FlushForTesting() {
  endpoint_client_->FlushForTesting();  // IN-TEST
}

AssociatedReceiverBase::~AssociatedReceiverBase() = default;

void AssociatedReceiverBase::BindImpl(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    std::unique_ptr<MessageReceiver> payload_validator,
    base::span<const uint32_t> sync_method_ordinals,
    scoped_refptr<base::SequencedTaskRunner> runner,
    uint32_t interface_version,
    const char* interface_name,
    MessageToMethodInfoCallback method_info_callback,
    MessageToMethodNameCallback method_name_callback) {
  DCHECK(handle.is_valid());

  endpoint_client_ = std::make_unique<InterfaceEndpointClient>(
      std::move(handle), receiver, std::move(payload_validator),
      sync_method_ordinals,
      internal::GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(runner)),
      interface_version, interface_name, method_info_callback,
      method_name_callback);
}

}  // namespace internal

void AssociateWithDisconnectedPipe(ScopedInterfaceEndpointHandle handle) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router =
      internal::MultiplexRouter::CreateAndStartReceiving(
          std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
          false, base::SequencedTaskRunner::GetCurrentDefault());
  router->AssociateInterface(std::move(handle));
}

}  // namespace mojo

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_receiver.h"

#include "base/sequenced_task_runner.h"
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
                                             const std::string& description) {
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

void AssociatedReceiverBase::FlushForTesting() {
  endpoint_client_->FlushForTesting();  // IN-TEST
}

AssociatedReceiverBase::~AssociatedReceiverBase() = default;

void AssociatedReceiverBase::BindImpl(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    std::unique_ptr<MessageReceiver> payload_validator,
    bool expect_sync_requests,
    scoped_refptr<base::SequencedTaskRunner> runner,
    uint32_t interface_version,
    const char* interface_name) {
  DCHECK(handle.is_valid());

  endpoint_client_.reset(new InterfaceEndpointClient(
      std::move(handle), receiver, std::move(payload_validator),
      expect_sync_requests,
      internal::GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(runner)),
      interface_version, interface_name));
}

}  // namespace internal

void AssociateWithDisconnectedPipe(ScopedInterfaceEndpointHandle handle) {
  MessagePipe pipe;
  scoped_refptr<internal::MultiplexRouter> router =
      new internal::MultiplexRouter(
          std::move(pipe.handle0), internal::MultiplexRouter::MULTI_INTERFACE,
          false, base::SequencedTaskRunnerHandle::Get());
  router->AssociateInterface(std::move(handle));
}

}  // namespace mojo

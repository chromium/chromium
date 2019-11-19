// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/associated_binding.h"

#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"

namespace mojo {

AssociatedBindingBase::AssociatedBindingBase() {}

AssociatedBindingBase::~AssociatedBindingBase() {}

void AssociatedBindingBase::SetFilter(std::unique_ptr<MessageFilter> filter) {
  DCHECK(endpoint_client_);
  endpoint_client_->SetFilter(std::move(filter));
}

void AssociatedBindingBase::Close() {
  endpoint_client_.reset();
}

void AssociatedBindingBase::CloseWithReason(uint32_t custom_reason,
                                            const std::string& description) {
  if (endpoint_client_)
    endpoint_client_->CloseWithReason(custom_reason, description);
  Close();
}

void AssociatedBindingBase::set_connection_error_handler(
    base::OnceClosure error_handler) {
  DCHECK(is_bound());
  endpoint_client_->set_connection_error_handler(std::move(error_handler));
}

void AssociatedBindingBase::set_connection_error_with_reason_handler(
    ConnectionErrorWithReasonCallback error_handler) {
  DCHECK(is_bound());
  endpoint_client_->set_connection_error_with_reason_handler(
      std::move(error_handler));
}

void AssociatedBindingBase::FlushForTesting() {
  endpoint_client_->FlushForTesting();
}

void AssociatedBindingBase::BindImpl(
    ScopedInterfaceEndpointHandle handle,
    MessageReceiverWithResponderStatus* receiver,
    std::unique_ptr<MessageReceiver> payload_validator,
    bool expect_sync_requests,
    scoped_refptr<base::SequencedTaskRunner> runner,
    uint32_t interface_version,
    const char* interface_name) {
  if (!handle.is_valid()) {
    endpoint_client_.reset();
    return;
  }

  endpoint_client_.reset(new InterfaceEndpointClient(
      std::move(handle), receiver, std::move(payload_validator),
      expect_sync_requests,
      internal::GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(runner)),
      interface_version, interface_name));
}

}  // namespace mojo

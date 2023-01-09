// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/associated_interface_ptr_state.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"

namespace mojo {
namespace internal {

AssociatedInterfacePtrStateBase::AssociatedInterfacePtrStateBase() = default;

AssociatedInterfacePtrStateBase::~AssociatedInterfacePtrStateBase() = default;

void AssociatedInterfacePtrStateBase::QueryVersion(
    base::OnceCallback<void(uint32_t)> callback) {
  // It is safe to capture |this| because the callback won't be run after this
  // object goes away.
  endpoint_client_->QueryVersion(
      base::BindOnce(&AssociatedInterfacePtrStateBase::OnQueryVersion,
                     base::Unretained(this), std::move(callback)));
}

void AssociatedInterfacePtrStateBase::RequireVersion(uint32_t version) {
  if (version <= version_)
    return;

  version_ = version;
  endpoint_client_->RequireVersion(version);
}

void AssociatedInterfacePtrStateBase::OnQueryVersion(
    base::OnceCallback<void(uint32_t)> callback,
    uint32_t version) {
  version_ = version;
  std::move(callback).Run(version);
}

void AssociatedInterfacePtrStateBase::FlushForTesting() {
  endpoint_client_->FlushForTesting();
}

void AssociatedInterfacePtrStateBase::CloseWithReason(
    uint32_t custom_reason,
    const std::string& description) {
  endpoint_client_->CloseWithReason(custom_reason, description);
}

void AssociatedInterfacePtrStateBase::Swap(
    AssociatedInterfacePtrStateBase* other) {
  using std::swap;
  swap(other->endpoint_client_, endpoint_client_);
  swap(other->version_, version_);
}

void AssociatedInterfacePtrStateBase::Bind(
    ScopedInterfaceEndpointHandle handle,
    uint32_t version,
    std::unique_ptr<MessageReceiver> validator,
    scoped_refptr<base::SequencedTaskRunner> runner,
    const char* interface_name,
    MessageToMethodInfoCallback method_info_callback,
    MessageToMethodNameCallback method_name_callback) {
  DCHECK(!endpoint_client_);
  DCHECK_EQ(0u, version_);
  DCHECK(handle.is_valid());

  version_ = version;
  // The version is only queried from the client so the value passed here
  // will not be used.
  endpoint_client_ = std::make_unique<InterfaceEndpointClient>(
      std::move(handle), nullptr, std::move(validator),
      /*sync_method_ordinals=*/base::span<const uint32_t>(),
      GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(runner)), 0u,
      interface_name, method_info_callback, method_name_callback);
}

ScopedInterfaceEndpointHandle AssociatedInterfacePtrStateBase::PassHandle() {
  auto handle = endpoint_client_->PassHandle();
  endpoint_client_.reset();
  return handle;
}

}  // namespace internal
}  // namespace mojo

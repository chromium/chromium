// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/interface_ptr_state.h"

#include <stdint.h>

#include <utility>

#include "base/containers/span.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/lib/task_runner_helper.h"

namespace mojo {
namespace internal {

InterfacePtrStateBase::InterfacePtrStateBase() = default;

InterfacePtrStateBase::~InterfacePtrStateBase() {
  endpoint_client_.reset();
  if (router_)
    router_->CloseMessagePipe();
}

void InterfacePtrStateBase::QueryVersion(
    base::OnceCallback<void(uint32_t)> callback) {
  // It is safe to capture |this| because the callback won't be run after this
  // object goes away.
  endpoint_client_->QueryVersion(
      base::BindOnce(&InterfacePtrStateBase::OnQueryVersion,
                     base::Unretained(this), std::move(callback)));
}

void InterfacePtrStateBase::RequireVersion(uint32_t version) {
  if (version <= version_)
    return;

  version_ = version;
  endpoint_client_->RequireVersion(version);
}

void InterfacePtrStateBase::PauseReceiverUntilFlushCompletes(
    PendingFlush flush) {
  router_->PausePeerUntilFlushCompletes(std::move(flush));
}

void InterfacePtrStateBase::FlushAsync(AsyncFlusher flusher) {
  router_->FlushAsync(std::move(flusher));
}

void InterfacePtrStateBase::Swap(InterfacePtrStateBase* other) {
  using std::swap;
  swap(other->router_, router_);
  swap(other->endpoint_client_, endpoint_client_);
  handle_.swap(other->handle_);
  runner_.swap(other->runner_);
  swap(other->version_, version_);
}

void InterfacePtrStateBase::Bind(
    PendingRemoteState* remote_state,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  DCHECK(!router_);
  DCHECK(!endpoint_client_);
  DCHECK(!handle_.is_valid());
  DCHECK_EQ(0u, version_);
  DCHECK(remote_state->pipe.is_valid());

  handle_ = std::move(remote_state->pipe);
  version_ = remote_state->version;
  runner_ =
      GetTaskRunnerToUseFromUserProvidedTaskRunner(std::move(task_runner));
}

PendingRemoteState InterfacePtrStateBase::Unbind() {
  return PendingRemoteState(PassMessagePipe(), version());
}

void InterfacePtrStateBase::OnQueryVersion(
    base::OnceCallback<void(uint32_t)> callback,
    uint32_t version) {
  version_ = version;
  std::move(callback).Run(version);
}

bool InterfacePtrStateBase::InitializeEndpointClient(
    bool passes_associated_kinds,
    bool has_sync_methods,
    bool has_uninterruptable_methods,
    std::unique_ptr<MessageReceiver> payload_validator,
    const char* interface_name,
    MessageToMethodInfoCallback method_info_callback,
    MessageToMethodNameCallback method_name_callback) {
  // The object hasn't been bound.
  if (!handle_.is_valid())
    return false;

  MultiplexRouter::Config config =
      (passes_associated_kinds || has_uninterruptable_methods)
          ? MultiplexRouter::MULTI_INTERFACE
          : (has_sync_methods
                 ? MultiplexRouter::SINGLE_INTERFACE_WITH_SYNC_METHODS
                 : MultiplexRouter::SINGLE_INTERFACE);
  router_ = MultiplexRouter::Create(std::move(handle_), config, true, runner_,
                                    interface_name);
  endpoint_client_ = std::make_unique<InterfaceEndpointClient>(
      router_->CreateLocalEndpointHandle(kPrimaryInterfaceId), nullptr,
      std::move(payload_validator),
      /* sync_method_ordinals= */ base::span<const uint32_t>(),
      std::move(runner_),
      // The version is only queried from the client so the value passed here
      // will not be used.
      0u, interface_name, method_info_callback, method_name_callback);

  // Note that we defer this until after attaching the endpoint. This is in case
  // `runner_` does not run tasks in the current sequence but MultiplexRouter is
  // in SINGLE_INTERFACE mode. In that case, MultiplexRouter elides some
  // internal synchronization, so we need to ensure that messages aren't
  // processed by the router before the endpoint above is fully attached.
  router_->StartReceiving();

  return true;
}

}  // namespace internal
}  // namespace mojo

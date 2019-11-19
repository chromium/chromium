// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/agent_impl.h"

#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"

namespace cr_fuchsia {

AgentImpl::ComponentStateBase::~ComponentStateBase() = default;

AgentImpl::ComponentStateBase::ComponentStateBase(
    base::StringPiece component_id)
    : component_id_(component_id) {
  fidl::InterfaceHandle<::fuchsia::io::Directory> directory;
  outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
      fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
      directory.NewRequest().TakeChannel());
  service_provider_ = std::make_unique<base::fuchsia::ServiceProviderImpl>(
      std::move(directory));

  // Tear down this instance when the client disconnects from the directory.
  service_provider_->SetOnLastClientDisconnectedClosure(base::BindOnce(
      &ComponentStateBase::TeardownIfUnused, base::Unretained(this)));
}

void AgentImpl::ComponentStateBase::DisconnectClientsAndTeardown() {
  agent_impl_->DeleteComponentState(component_id_);
  // Do not touch |this|, since it is already gone.
}

void AgentImpl::ComponentStateBase::TeardownIfUnused() {
  DCHECK(agent_impl_);

  // Don't teardown if the ServiceProvider has client(s).
  if (service_provider_->has_clients())
    return;

  // Don't teardown if caller-specified bindings still have clients.
  for (auto& keepalive_callback : keepalive_callbacks_) {
    if (keepalive_callback.Run())
      return;
  }

  DisconnectClientsAndTeardown();
  // Do not touch |this|, since it is already gone.
}

AgentImpl::AgentImpl(
    sys::OutgoingDirectory* outgoing_directory,
    CreateComponentStateCallback create_component_state_callback)
    : create_component_state_callback_(
          std::move(create_component_state_callback)),
      agent_binding_(outgoing_directory, this) {}

AgentImpl::~AgentImpl() {
  DCHECK(active_components_.empty());
}

void AgentImpl::Connect(
    std::string requester_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services) {
  auto it = active_components_.find(requester_url);
  if (it == active_components_.end()) {
    std::unique_ptr<ComponentStateBase> component_state =
        create_component_state_callback_.Run(requester_url);
    if (!component_state)
      return;

    auto result =
        active_components_.emplace(requester_url, std::move(component_state));
    it = result.first;
    CHECK(result.second);
    it->second->agent_impl_ = this;
  }
  it->second->service_provider_->AddBinding(std::move(services));
}

void AgentImpl::DeleteComponentState(base::StringPiece component_id) {
  size_t removed_components = active_components_.erase(component_id);
  DCHECK_EQ(removed_components, 1u);
}

}  // namespace cr_fuchsia

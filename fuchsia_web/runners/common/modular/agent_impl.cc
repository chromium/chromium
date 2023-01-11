// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/modular/agent_impl.h"

#include <lib/fdio/directory.h>
#include <lib/sys/cpp/component_context.h>

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"

namespace cr_fuchsia {

AgentImpl::ComponentStateBase::~ComponentStateBase() = default;

AgentImpl::ComponentStateBase::ComponentStateBase(
    base::StringPiece component_id)
    : component_id_(component_id) {
  service_provider_ = base::ServiceProviderImpl::CreateForOutgoingDirectory(
      &outgoing_directory_);

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
    : AgentImpl(outgoing_directory,
                std::move(create_component_state_callback),
                {}) {}

AgentImpl::AgentImpl(
    sys::OutgoingDirectory* outgoing_directory,
    CreateComponentStateCallback create_component_state_callback,
    std::vector<std::string> public_service_names)
    : create_component_state_callback_(
          std::move(create_component_state_callback)),
      public_service_names_(std::move(public_service_names)),
      agent_binding_(outgoing_directory, this) {
  if (!public_service_names_.empty()) {
    fuchsia::io::DirectoryHandle root_directory;
    zx_status_t status = outgoing_directory->Serve(root_directory.NewRequest());
    ZX_CHECK(status == ZX_OK, status) << "Serve(root)";
    fuchsia::io::DirectoryHandle svc_directory;
    status = fdio_service_connect_at(
        root_directory.channel().get(), "svc",
        svc_directory.NewRequest().TakeChannel().release());
    ZX_CHECK(status == ZX_OK, status) << "open(svc)";
    public_services_ =
        std::make_unique<sys::ServiceDirectory>(std::move(svc_directory));
  }
}

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

    // Add public services to the |component_state|.
    const auto* outgoing = component_state->outgoing_directory();
    for (const auto& service_name : public_service_names_) {
      zx_status_t status = outgoing->AddPublicService(
          std::make_unique<vfs::Service>(
              [public_services = public_services_.get(), service_name](
                  zx::channel request, async_dispatcher_t* dispatcher) {
                public_services->Connect(service_name, std::move(request));
              }),
          service_name);
      CHECK_EQ(status, ZX_OK);
    }

    // Register the new component's state.
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

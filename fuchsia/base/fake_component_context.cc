// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/fake_component_context.h"

#include <fuchsia/base/agent_impl.h>

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"

namespace cr_fuchsia {

FakeComponentContext::FakeComponentContext(
    AgentImpl::CreateComponentStateCallback create_component_state_callback,
    sys::OutgoingDirectory* outgoing_directory,
    base::StringPiece component_url)
    : binding_(outgoing_directory, this),
      // Publishing the Agent to |outgoing_directory| is not necessary, but
      // also shouldn't do any harm.
      agent_impl_(outgoing_directory,
                  std::move(create_component_state_callback)),
      component_url_(component_url.as_string()) {}

void FakeComponentContext::ConnectToAgent(
    std::string agent_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> controller) {
  agent_impl_.Connect(component_url_, std::move(services));
}

void FakeComponentContext::ConnectToAgentService(
    fuchsia::modular::AgentServiceRequest request) {
  if (!agent_services_) {
    ConnectToAgent(component_url_, agent_services_.NewRequest(), nullptr);
  }
  agent_services_->ConnectToService(std::move(request.service_name()),
                                    std::move(*request.mutable_channel()));
}

void FakeComponentContext::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << " API: " << name;
}

FakeComponentContext::~FakeComponentContext() {
  agent_services_.Unbind();
  base::RunLoop().RunUntilIdle();
}

}  // namespace cr_fuchsia

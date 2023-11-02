// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/common/modular/fake_component_context.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "fuchsia_web/runners/common/modular/agent_impl.h"

namespace cr_fuchsia {

FakeComponentContext::FakeComponentContext(
    sys::OutgoingDirectory* outgoing_directory,
    base::StringPiece component_url)
    : binding_(outgoing_directory, this),
      component_url_(component_url),
      outgoing_directory_(outgoing_directory) {}

void FakeComponentContext::RegisterCreateComponentStateCallback(
    base::StringPiece agent_url,
    AgentImpl::CreateComponentStateCallback create_component_state_callback) {
  agent_impl_map_.insert(std::make_pair(
      agent_url,
      std::make_unique<AgentImpl>(outgoing_directory_,
                                  std::move(create_component_state_callback))));
}

void FakeComponentContext::DeprecatedConnectToAgent(
    std::string agent_url,
    fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services,
    fidl::InterfaceRequest<fuchsia::modular::AgentController> controller) {
  auto it = agent_impl_map_.find(agent_url);
  CHECK(it != agent_impl_map_.end())
      << "Received request for an unknown agent URL: " << agent_url;

  it->second->Connect(component_url_, std::move(services));
}

void FakeComponentContext::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << " API: " << name;
}

FakeComponentContext::~FakeComponentContext() {
  agent_services_.Unbind();
  base::RunLoop().RunUntilIdle();
}

}  // namespace cr_fuchsia

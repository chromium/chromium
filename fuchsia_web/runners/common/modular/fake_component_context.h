// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_FAKE_COMPONENT_CONTEXT_H_
#define FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_FAKE_COMPONENT_CONTEXT_H_

#include <fuchsia/modular/cpp/fidl_test_base.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "fuchsia_web/runners/common/modular/agent_impl.h"

namespace cr_fuchsia {

// Used to test interactions with an Agent in unit-tests for a component.
// |create_component_state_callback| can be used with a test-specific
// ComponentStateBase to serve fake services to the component.
// |outgoing_directory| specifies the directory into which the ComponentContext
// should be published, alongside any other services the test wishes to provide
// to the component's default service namespace. |component_url| specifies the
// component identity that should be reported to the Agent
class FakeComponentContext
    : public fuchsia::modular::testing::ComponentContext_TestBase {
 public:
  FakeComponentContext(sys::OutgoingDirectory* outgoing_directory,
                       base::StringPiece component_url);

  FakeComponentContext(const FakeComponentContext&) = delete;
  FakeComponentContext& operator=(const FakeComponentContext&) = delete;

  ~FakeComponentContext() override;

  void RegisterCreateComponentStateCallback(
      base::StringPiece agent_url,
      AgentImpl::CreateComponentStateCallback callback);

  // fuchsia::modular::ComponentContext_TestBase implementation.
  void DeprecatedConnectToAgent(
      std::string agent_url,
      fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services,
      fidl::InterfaceRequest<fuchsia::modular::AgentController> controller)
      override;
  void NotImplemented_(const std::string& name) override;

 private:
  base::ScopedServiceBinding<fuchsia::modular::ComponentContext> binding_;
  const std::string component_url_;
  sys::OutgoingDirectory* const outgoing_directory_;
  fuchsia::sys::ServiceProviderPtr agent_services_;

  std::map<base::StringPiece, std::unique_ptr<AgentImpl>> agent_impl_map_;
};

}  // namespace cr_fuchsia
#endif  // FUCHSIA_WEB_RUNNERS_COMMON_MODULAR_FAKE_COMPONENT_CONTEXT_H_

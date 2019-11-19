// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_
#define FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/fuchsia/startup_context.h"
#include "base/macros.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"

// sys::Runner which instantiates Cast activities specified via cast/casts URIs.
class CastRunner : public WebContentRunner {
 public:
  using OnDestructionCallback = base::OnceCallback<void(CastRunner*)>;

  // |outgoing_directory|: The directory that this CastRunner will publish
  //     itself to.
  // |context_feature_flags|: The feature flags to use when creating the
  //     runner's Context.
  CastRunner(sys::OutgoingDirectory* outgoing_directory,
             fuchsia::web::CreateContextParams create_context_params);

  ~CastRunner() override;

  // WebContentRunner implementation.
  void DestroyComponent(WebComponent* component) override;

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Used to connect to the CastAgent to access Cast-specific services.
  static const char kAgentComponentUrl[];

  // Returns the number of active CastRunner instances.
  size_t GetChildCastRunnerCountForTest();

 private:
  // Constructor used for creating CastRunners that run apps in dedicated
  // Contexts. Child CastRunners may only spawn one Component and will be
  // destroyed by their parents when their singleton Components are destroyed.
  // |on_destruction_callback| is invoked when the child component is destroyed.
  CastRunner(OnDestructionCallback on_destruction_callback,
             fuchsia::web::ContextPtr context);

  // Starts a component once all configuration data is available.
  void MaybeStartComponent(
      CastComponent::CastComponentParams* pending_component_params);

  // Cancels the launch of a component.
  void CancelComponentLaunch(CastComponent::CastComponentParams* params);

  void CreateAndRegisterCastComponent(
      CastComponent::CastComponentParams params);
  void GetConfigCallback(CastComponent::CastComponentParams* pending_component,
                         chromium::cast::ApplicationConfig app_config);
  void GetBindingsCallback(
      CastComponent::CastComponentParams* pending_component,
      std::vector<chromium::cast::ApiBinding> bindings);
  void OnChildRunnerDestroyed(CastRunner* cast_runner);
  fuchsia::web::ContextPtr CreateCastRunnerWebContext();

  // Creates a CastRunner configured to serve data from content directories in
  // |params|.
  CastRunner* CreateChildRunnerForIsolatedComponent(
      CastComponent::CastComponentParams* params);

  // Holds StartComponent() requests while the ApplicationConfig is being
  // fetched from the ApplicationConfigManager.
  base::flat_set<std::unique_ptr<CastComponent::CastComponentParams>,
                 base::UniquePtrComparator>
      pending_components_;

  // Used for creating the CastRunner's ContextPtr.
  fuchsia::web::CreateContextParams create_context_params_;

  // Used as a template for creating the ContextPtrs of isolated Runners.
  fuchsia::web::CreateContextParams common_create_context_params_;

  // Invoked upon destruction of "isolated" runners, used to signal termination
  // to parents.
  OnDestructionCallback on_destruction_callback_;

  // Manages isolated CastRunners owned by |this| instance.
  base::flat_set<std::unique_ptr<CastRunner>, base::UniquePtrComparator>
      isolated_runners_;

  DISALLOW_COPY_AND_ASSIGN(CastRunner);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_RUNNER_H_

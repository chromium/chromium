// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <memory>

#include "base/callback.h"
#include "base/values.h"
#include "fuchsia/engine/web_engine_export.h"

class WEB_ENGINE_EXPORT ContextProviderImpl
    : public fuchsia::web::ContextProvider,
      public fuchsia::web::Debug {
 public:
  // Component URL used to launch WebEngine instances to host Contexts.
  static const char kWebInstanceComponentUrl[];

  ContextProviderImpl();
  ~ContextProviderImpl() override;

  ContextProviderImpl(const ContextProviderImpl&) = delete;
  ContextProviderImpl& operator=(const ContextProviderImpl&) = delete;

  // fuchsia::web::ContextProvider implementation.
  void Create(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::web::Context> context_request) override;

  // Sets a config to use for the test, instead of looking for the config file.
  void set_config_for_test(base::Value config) {
    config_for_test_ = std::move(config);
  }

 private:
  // fuchsia::web::Debug implementation.
  void EnableDevTools(
      fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
      EnableDevToolsCallback callback) override;

  // Returns the Launcher for the isolated Environment in which web instances
  // should run. If the Environment does not presently exist then it will be
  // created.
  fuchsia::sys::Launcher* IsolatedEnvironmentLauncher();

  // Set by configuration tests.
  base::Value config_for_test_;

  // The DevToolsListeners registered via the Debug interface.
  fidl::InterfacePtrSet<fuchsia::web::DevToolsListener> devtools_listeners_;

  // Used to manage the isolated Environment that web instances run in.
  fuchsia::sys::LauncherPtr isolated_environment_launcher_;
  fuchsia::sys::EnvironmentControllerPtr isolated_environment_controller_;
};

#endif  // FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "fuchsia/engine/web_engine_export.h"

namespace base {
class CommandLine;
struct LaunchOptions;
class Process;
}  // namespace base

class WEB_ENGINE_EXPORT ContextProviderImpl
    : public fuchsia::web::ContextProvider,
      public fuchsia::web::Debug {
 public:
  using LaunchCallbackForTest = base::RepeatingCallback<base::Process(
      const base::CommandLine& command,
      const base::LaunchOptions& options)>;

  // Handle Id used to pass the request channel to Context processes.
  static const uint32_t kContextRequestHandleId;

  ContextProviderImpl();
  ~ContextProviderImpl() override;

  // fuchsia::web::ContextProvider implementation.
  void Create(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::web::Context> context_request) override;

  // Sets a |launch| callback to use instead of calling LaunchProcess() to
  // create Context processes.
  void SetLaunchCallbackForTest(LaunchCallbackForTest launch);

 private:
  // fuchsia::web::Debug implementation.
  void EnableDevTools(
      fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
      EnableDevToolsCallback callback) override;

  // Set by tests to use to launch Context child processes, e.g. to allow a
  // fake Context process to be launched.
  LaunchCallbackForTest launch_for_test_;

  // The DevToolsListeners registered via the Debug interface.
  fidl::InterfacePtrSet<fuchsia::web::DevToolsListener> devtools_listeners_;

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImpl);
};

#endif  // FUCHSIA_ENGINE_CONTEXT_PROVIDER_IMPL_H_

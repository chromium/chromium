// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_V1_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_V1_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/command_line.h"
#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"

// Helper class that allows web_instance Components to be launched based on
// caller-supplied |CreateContextParams|.
//
// Note that Components using this class must:
// 1. Include the "web_instance.cmx" in their package, for the implementation
//    to read the sandbox services from.
// 2. List the fuchsia.sys.Environment & .Loader services in their sandbox.
// 3. Have web_engine's config-data available to the calling Component.
//    TODO(crbug.com/1212191): Make web_instance read the config & remove this.
//
// To ensure proper product data registration, Components using the class must:
// * Have the same version and channel as WebEngine.
// * Include the following services in their manifest:
//   * "fuchsia.feedback.ComponentDataRegister"
//   * "fuchsia.feedback.CrashReportingProductRegister"
// * Instantiate the class on a thread with an async_dispatcher.
// TODO(crbug.com/1211174): Remove these requirements.
class WebInstanceHostV1 {
 public:
  WebInstanceHostV1();
  ~WebInstanceHostV1();

  WebInstanceHostV1(const WebInstanceHostV1&) = delete;
  WebInstanceHostV1& operator=(const WebInstanceHostV1&) = delete;

  // Creates a new web_instance Component and connects |services_request| to it.
  // Returns ZX_OK if |params| were valid, and the Component was launched.
  // Appends to the given |extra_args|.
  // Use base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM) for
  // empty args.
  zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      base::CommandLine extra_args);

  // Exposes a fuchsia.web.Debug protocol implementation that can be used
  // to receive notifications of DevTools debug ports for new web instances.
  fuchsia::web::Debug* debug_api();

  // The next created WebInstance will have access to the given directory handle
  // for temporary directory reading and writing.
  // Ownership of the directory is passed to the next created instance.
  void set_tmp_dir(fuchsia::io::DirectoryHandle tmp_dir) {
    tmp_dir_ = std::move(tmp_dir);
  }

 private:
  // Returns the Launcher for the isolated Environment in which web instances
  // should run. If the Environment does not presently exist then it will be
  // created.
  fuchsia::sys::Launcher* IsolatedEnvironmentLauncher();

  // Used to manage the isolated Environment that web instances run in.
  fuchsia::sys::LauncherPtr isolated_environment_launcher_;
  fuchsia::sys::EnvironmentControllerPtr isolated_environment_controller_;

  // Controllers per each subcomponent launched by this host.
  fidl::InterfacePtrSet<fuchsia::sys::ComponentController>
      component_controller_set_;

  // Implements the fuchsia.web.Debug API across all instances.
  FuchsiaWebDebugProxy debug_proxy_;

  // If set, then the next created WebInstance will gain ownership of this
  // directory.
  fuchsia::io::DirectoryHandle tmp_dir_;
};

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_V1_H_

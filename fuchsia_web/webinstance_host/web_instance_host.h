// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include "base/command_line.h"
#include "base/values.h"

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
class WebInstanceHost {
 public:
  WebInstanceHost();
  ~WebInstanceHost();

  WebInstanceHost(const WebInstanceHost&) = delete;
  WebInstanceHost& operator=(const WebInstanceHost&) = delete;

  // Creates a new web_instance Component and connects |services_request| to it.
  // Returns ZX_OK if |params| were valid, and the Component was launched.
  // Appends to the given |extra_args|.
  // Use base::CommandLine(base::CommandLine::NoProgram::NO_PROGRAM) for
  // empty args.
  zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      base::CommandLine extra_args);

  // Enables/disables remote debugging mode in instances created by this host.
  // This may be called at any time, and will not affect pre-existing instances.
  void set_enable_remote_debug_mode(bool enable) {
    enable_remote_debug_mode_ = enable;
  }

  // Sets a set of config-data to use when launching instances, instead of any
  // system-provided config-data. May be called at any time, and will not
  // affect pre-existing instances.
  void set_config_for_test(base::Value config) {
    config_for_test_ = std::move(config);
  }

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

  // If true then new instances will have remote debug mode enabled.
  bool enable_remote_debug_mode_ = false;

  // If set, then the next created WebInstance will gain ownership of this
  // directory.
  fuchsia::io::DirectoryHandle tmp_dir_;

  // Set by configuration tests.
  base::Value config_for_test_;
};

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_

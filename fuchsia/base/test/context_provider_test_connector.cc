// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test/context_provider_test_connector.h"

#include <unistd.h>

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/sys/cpp/component_context.h>
#include <zircon/processargs.h>
#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

namespace cr_fuchsia {

namespace {

// |is_for_logging_test| should only be true when testing WebEngine's logging
// behavior as it prevents WebEngine logs from being included in the test
// output. When false, WebEngine logs are not included in the Fuchsia system
// log.
fidl::InterfaceHandle<fuchsia::io::Directory> StartWebEngineForTestsInternal(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line,
    bool is_for_logging_test) {
  DCHECK(command_line.argv()[0].empty()) << "Must use NO_PROGRAM.";

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url =
      "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cmx";
  // Add all switches and arguments, skipping the program.
  launch_info.arguments.emplace(std::vector<std::string>(
      command_line.argv().begin() + 1, command_line.argv().end()));

  if (!is_for_logging_test) {
    // Clone stderr from the current process to WebEngine and ask it to
    // redirect all logs to stderr.
    launch_info.err = fuchsia::sys::FileDescriptor::New();
    launch_info.err->type0 = PA_FD;
    zx_status_t status = fdio_fd_clone(
        STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
    ZX_CHECK(status == ZX_OK, status);
    launch_info.arguments->push_back("--enable-logging=stderr");
  }

  fuchsia::io::DirectorySyncPtr web_engine_services_dir;
  launch_info.directory_request =
      web_engine_services_dir.NewRequest().TakeChannel();

  fuchsia::sys::LauncherPtr launcher;
  base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            std::move(component_controller_request));

  // The WebEngine binary can take sufficiently long for blobfs to resolve that
  // tests using it may timeout as a result. Wait for the ContextProvider to
  // be responsive, by making a synchronous request to its service directory.
  fuchsia::io::NodeAttributes attributes{};
  zx_status_t stat{};
  zx_status_t status = web_engine_services_dir->GetAttr(&stat, &attributes);
  ZX_CHECK(status == ZX_OK, status);

  return web_engine_services_dir.Unbind();
}

}  // namespace

fuchsia::web::ContextProviderPtr ConnectContextProvider(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line) {
  sys::ServiceDirectory web_engine_service_dir(StartWebEngineForTestsInternal(
      std::move(component_controller_request), command_line, false));
  return web_engine_service_dir.Connect<fuchsia::web::ContextProvider>();
}

fuchsia::web::ContextProviderPtr ConnectContextProviderForLoggingTest(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& command_line) {
  sys::ServiceDirectory web_engine_service_dir(StartWebEngineForTestsInternal(
      std::move(component_controller_request), command_line, true));
  return web_engine_service_dir.Connect<fuchsia::web::ContextProvider>();
}
}  // namespace cr_fuchsia

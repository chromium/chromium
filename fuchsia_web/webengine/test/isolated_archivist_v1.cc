// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/isolated_archivist_v1.h"

#include <fuchsia/io/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

namespace {

// Starts an isolated instance of Archivist to receive and dump log statements
// via the fuchsia.logger.Log* APIs.
fidl::InterfaceHandle<fuchsia::io::Directory> StartIsolatedArchivist(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request) {
  constexpr char kArchivistUrl[] =
      "fuchsia-pkg://fuchsia.com/archivist-for-embedding#meta/"
      "archivist-for-embedding.cmx";

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = kArchivistUrl;

  fidl::InterfaceHandle<fuchsia::io::Directory> archivist_services_dir;
  launch_info.directory_request = archivist_services_dir.NewRequest();

  auto launcher = base::ComponentContextForProcess()
                      ->svc()
                      ->Connect<fuchsia::sys::Launcher>();
  launcher->CreateComponent(std::move(launch_info),
                            std::move(component_controller_request));

  return archivist_services_dir;
}

}  // namespace

IsolatedArchivist::IsolatedArchivist(
    ::sys::OutgoingDirectory& outgoing_directory) {
  ::sys::ServiceDirectory archivist_service_dir(
      StartIsolatedArchivist(archivist_controller_.NewRequest()));

  zx_status_t status = archivist_service_dir.Connect(log_.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect to Log";

  // Redirect the LogSink service to the isolated archivist instance.
  status = outgoing_directory.RemovePublicService<fuchsia::logger::LogSink>();
  ZX_CHECK(status == ZX_OK, status) << "Remove LogSink service";

  log_sink_publisher_.emplace(
      &outgoing_directory,
      ::fidl::InterfaceRequestHandler<fuchsia::logger::LogSink>(
          [service_directory = std::move(archivist_service_dir)](auto request) {
            service_directory.Connect(std::move(request));
          }));
}

IsolatedArchivist::~IsolatedArchivist() = default;

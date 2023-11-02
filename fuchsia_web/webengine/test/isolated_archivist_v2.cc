// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/isolated_archivist_v2.h"

#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

IsolatedArchivist::IsolatedArchivist(
    ::sys::OutgoingDirectory& outgoing_directory) {
  // Redirect the LogSink service to the isolated archivist instance.
  zx_status_t status =
      outgoing_directory.RemovePublicService<fuchsia::logger::LogSink>();
  ZX_CHECK(status == ZX_OK, status) << "Remove LogSink service";

  auto service_directory = base::ComponentContextForProcess()->svc();
  log_sink_publisher_.emplace(
      &outgoing_directory,
      ::fidl::InterfaceRequestHandler<fuchsia::logger::LogSink>(
          [service_directory](auto request) {
            service_directory->Connect(std::move(request),
                                       "fuchsia.logger.LogSink.isolated");
          }));

  status = service_directory->Connect<fuchsia::logger::Log>(
      log_.NewRequest(), "fuchsia.logger.Log.isolated");
  ZX_CHECK(status == ZX_OK, status) << "Connect to Log.isolated";
}

IsolatedArchivist::~IsolatedArchivist() = default;

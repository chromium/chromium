// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/isolated_archivist.h"

#include <lib/async/default.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"

IsolatedArchivist::IsolatedArchivist(
    ::sys::OutgoingDirectory& outgoing_directory) {
  // Redirect the LogSink service to the isolated archivist instance.
  zx_status_t status =
      outgoing_directory.RemovePublicService<fuchsia_logger::LogSink>(
          fidl::DiscoverableProtocolName<fuchsia_logger::LogSink>);
  ZX_CHECK(status == ZX_OK, status) << "Remove LogSink service";

  auto service_directory = base::ComponentContextForProcess()->svc();
  log_sink_publisher_.emplace(
      &outgoing_directory,
      [](fidl::ServerEnd<fuchsia_logger::LogSink> server_end) {
        auto result = base::fuchsia_component::Connect(
            std::move(server_end), "fuchsia.logger.LogSink.isolated");
        if (result.is_error()) {
          ZX_DLOG(ERROR, result.status_value())
              << "Failed to connect to fuchsia.logger.LogSink.isolated";
        }
      });

  auto log_client_end = base::fuchsia_component::Connect<fuchsia_logger::Log>(
      "fuchsia.logger.Log.isolated");
  ZX_CHECK(log_client_end.is_ok(), log_client_end.status_value());
  log_.Bind(std::move(log_client_end.value()), async_get_default_dispatcher());
}

IsolatedArchivist::~IsolatedArchivist() = default;

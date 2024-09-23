// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_H_

#include <fidl/fuchsia.logger/cpp/fidl.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include <optional>

#include "base/fuchsia/scoped_service_publisher.h"

// Runs an isolated archivist-for-embedding, publishing its
// fuchsia_logger::LogSink into a given OutgoingDirectory, and providing access
// to its fuchsia.logger.Log. Consumers of this class must use
// `//build/config/fuchsia/test/archivist.shard.test-cml` in their component
// manifest.
class IsolatedArchivist {
 public:
  explicit IsolatedArchivist(sys::OutgoingDirectory& outgoing_directory);
  IsolatedArchivist(const IsolatedArchivist&) = delete;
  IsolatedArchivist& operator=(const IsolatedArchivist&) = delete;
  ~IsolatedArchivist();

  fidl::Client<fuchsia_logger::Log>& log() { return log_; }

 private:
  std::optional<base::ScopedNaturalServicePublisher<fuchsia_logger::LogSink>>
      log_sink_publisher_;
  fidl::Client<fuchsia_logger::Log> log_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_H_

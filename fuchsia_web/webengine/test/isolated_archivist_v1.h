// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_V1_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_V1_H_

#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include "base/fuchsia/scoped_service_publisher.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Runs an isolated archivist-for-embedding, publishing its
// fuchsia::logger::LogSink into a given OutgoingDirectory, and providing access
// to its fuchsia.logger.Log.
class IsolatedArchivist {
 public:
  explicit IsolatedArchivist(::sys::OutgoingDirectory& outgoing_directory);
  IsolatedArchivist(const IsolatedArchivist& other) = delete;
  IsolatedArchivist& operator=(const IsolatedArchivist& other) = delete;
  ~IsolatedArchivist();

  ::fuchsia::logger::Log& log() { return *log_; }

 private:
  absl::optional<base::ScopedServicePublisher<fuchsia::logger::LogSink>>
      log_sink_publisher_;
  ::fuchsia::sys::ComponentControllerPtr archivist_controller_;
  ::fuchsia::logger::LogPtr log_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_ISOLATED_ARCHIVIST_V1_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_SCOPED_CONNECTION_CHECKER_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_SCOPED_CONNECTION_CHECKER_H_

#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/wire/connect_service.h>
#include <lib/vfs/cpp/service.h>

#include <memory>
#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// Verifies that a connection was made, or never attempted, for a given
// `Protocol` without needing to provide an implementation.
template <typename Protocol, bool expect_connection>
class ScopedConnectionCheckerBase {
 public:
  explicit ScopedConnectionCheckerBase(
      sys::OutgoingDirectory* outgoing_directory) {
    zx_status_t status = outgoing_directory->AddPublicService(
        std::make_unique<vfs::Service>(
            [this](zx::channel channel, async_dispatcher_t*) {
              pending_channels_.push_back(std::move(channel));
            }),
        fidl::DiscoverableProtocolName<Protocol>);

    ZX_CHECK(status == ZX_OK, status) << "OutgoingDirectory::AddPublicService";
  }

  ~ScopedConnectionCheckerBase() {
    if ((expect_connection && pending_channels_.empty()) ||
        (!expect_connection && !pending_channels_.empty())) {
      ADD_FAILURE();
    }
  }

  ScopedConnectionCheckerBase(const ScopedConnectionCheckerBase&) = delete;
  ScopedConnectionCheckerBase& operator=(const ScopedConnectionCheckerBase&) =
      delete;

 private:
  // Client channels are held in a pending (unconnected) state for the
  // lifetime of `this`, so that the client never sees a disconnection event.
  std::vector<zx::channel> pending_channels_;
};

// Checks that no client attempted to connect to `Protocol`.
template <typename Protocol>
using NeverConnectedChecker = ScopedConnectionCheckerBase<Protocol, false>;

// Checks that at least one client attempted to connect to `Protocol`.
template <typename Protocol>
using EnsureConnectedChecker = ScopedConnectionCheckerBase<Protocol, true>;

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_SCOPED_CONNECTION_CHECKER_H_

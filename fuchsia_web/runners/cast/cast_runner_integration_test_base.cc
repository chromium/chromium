// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_runner_integration_test_base.h"

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/fd.h>
#include <lib/sys/cpp/component_context.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/gtest/include/gtest/gtest.h"

CastRunnerIntegrationTest::CastRunnerIntegrationTest()
    : CastRunnerIntegrationTest(test::kCastRunnerFeaturesNone) {}

CastRunnerIntegrationTest::CastRunnerIntegrationTest(
    test::CastRunnerFeatures runner_features)
    : cast_runner_launcher_(runner_features) {}

CastRunnerIntegrationTest::~CastRunnerIntegrationTest() = default;

void CastRunnerIntegrationTest::SetUp() {
  cast_runner_services_ = cast_runner_launcher_.StartCastRunner();

  // Connect to the CastRunner's fuchsia.sys.Runner interface.
  cast_runner_ = cast_runner_services().Connect<fuchsia::sys::Runner>();
  cast_runner_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "CastRunner closed channel.";
    ADD_FAILURE();
  });

  static constexpr base::StringPiece kTestServerRoot(
      "fuchsia_web/runners/cast/testdata");
  test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
  net::test_server::RegisterDefaultHandlers(&test_server_);
  ASSERT_TRUE(test_server_.Start());
}

void CastRunnerIntegrationTest::TearDown() {
  // Unbind the Runner channel, to prevent it from triggering an error when
  // the CastRunner and WebEngine are torn down.
  cast_runner_.Unbind();
}

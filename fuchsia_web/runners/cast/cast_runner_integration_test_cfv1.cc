// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "fuchsia_web/runners/cast/cast_runner_integration_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

class CastRunnerCfv1ShimIntegrationTest : public CastRunnerIntegrationTest {
 public:
  CastRunnerCfv1ShimIntegrationTest()
      : CastRunnerIntegrationTest(test::kCastRunnerFeaturesCfv1Shim),
        fake_runner_publisher_(
            &cast_runner_launcher().services_for_runner(),
            [this](fidl::InterfaceRequest<fuchsia::sys::Runner> request) {
              received_requests_.push_back(std::move(request));
              if (on_request_received_)
                on_request_received_.Run();
            },
            "fuchsia.sys.Runner-cast") {}

  void RunUntilRequestsReceived(size_t expected_count) {
    base::RunLoop run_loop;
    base::AutoReset reset(&on_request_received_,
                          base::BindLambdaForTesting([&]() {
                            if (received_requests_.size() == expected_count)
                              run_loop.Quit();
                          }));
    run_loop.Run();
  }

 protected:
  std::vector<fidl::InterfaceRequest<fuchsia::sys::Runner>> received_requests_;
  base::RepeatingClosure on_request_received_;

 private:
  base::ScopedServicePublisher<fuchsia::sys::Runner> fake_runner_publisher_;
};

// Ensure that when running in CFv1 "shim" mode, all connection attempts are
// trivially redirected to a fuchsia.sys.Runner-cast service capability in
// the shim Runner's environment.
TEST_F(CastRunnerCfv1ShimIntegrationTest, ProxiesConnect) {
  ASSERT_EQ(received_requests_.size(), 0u);

  // The test constructor launched the CastRunner, configured as CFv1 shim,
  // and immediately connected to it. That should result in two requests via the
  // additional-services, which will be handled by |fake_runner_publisher_|
  // as soon as the message loop is allowed to pump events.
  // The first request is from the Runner shim itself, to allow it to monitor
  // whether the service capability is still valid.
  // The second is the test's connection to the shim Runner.
  RunUntilRequestsReceived(2u);
};

// Ensure that CFv1 "shim" mode tears down the Runner component if the
// underlying service capability disconnects it. This is required in order to
// have the shim correctly reflect instability in the real Runner, to the CFv1
// framework.
TEST_F(CastRunnerCfv1ShimIntegrationTest, ExitOnFailure) {
  // |cast_runner_| is expected to disconnect, so remove the error handler.
  cast_runner().set_error_handler([](zx_status_t) {});

  // Wait for the two incoming Runner connections.
  RunUntilRequestsReceived(2u);

  // Close the two connections, and expect the Runner to self-terminate.
  received_requests_.clear();
  base::RunLoop loop;
  cast_runner_launcher().controller_ptr().set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  loop.Run();
};

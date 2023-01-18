// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_INTEGRATION_TEST_BASE_H_
#define FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_INTEGRATION_TEST_BASE_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <stdint.h>

#include <memory>

#include "base/fuchsia/test_component_controller.h"
#include "base/location.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "fuchsia_web/runners/cast/test/cast_runner_features.h"
#include "fuchsia_web/runners/cast/test/cast_runner_launcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

// The base class for cast runner integration tests; templated on the type of
// launcher used to start the component. This allows the same tests to be used
// under both component framework v1 (using fuchsia.sys.Launcher) and v2
// (using component_testing::RealmBuilder).
class CastRunnerIntegrationTest : public testing::Test {
 public:
  CastRunnerIntegrationTest(const CastRunnerIntegrationTest&) = delete;
  CastRunnerIntegrationTest& operator=(const CastRunnerIntegrationTest&) =
      delete;

 protected:
  // Convenience constructor with `runner_features` == kCastRunnerFeaturesNone.
  CastRunnerIntegrationTest();
  explicit CastRunnerIntegrationTest(test::CastRunnerFeatures runner_features);
  ~CastRunnerIntegrationTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  test::CastRunnerLauncher& cast_runner_launcher() {
    return cast_runner_launcher_;
  }
  net::EmbeddedTestServer& test_server() { return test_server_; }
  fuchsia::sys::RunnerPtr& cast_runner() { return cast_runner_; }
  const sys::ServiceDirectory& cast_runner_services() const {
    return *cast_runner_services_;
  }

  FakeApplicationConfigManager& app_config_manager() {
    return cast_runner_launcher().fake_cast_agent().app_config_manager();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  // TODO(https://crbug.com/1168538): Override the RunLoop timeout set by
  // |task_environment_| to allow for the very high variability in web.Context
  // launch times.
  const base::test::ScopedRunLoopTimeout scoped_timeout_{
      FROM_HERE, TestTimeouts::action_max_timeout()};

  test::CastRunnerLauncher cast_runner_launcher_;

  fuchsia::sys::RunnerPtr cast_runner_;

  std::unique_ptr<sys::ServiceDirectory> cast_runner_services_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_CAST_RUNNER_INTEGRATION_TEST_BASE_H_

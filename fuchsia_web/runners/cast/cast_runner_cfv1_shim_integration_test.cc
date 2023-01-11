// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/fd.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_publisher.h"
#include "base/fuchsia/test_component_controller.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "fuchsia_web/runners/cast/cast_runner_switches.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class CastRunnerCfv1ShimIntegrationTest : public ::testing::Test {
 protected:
  CastRunnerCfv1ShimIntegrationTest()
      : fake_runner_publisher_(
            &services_for_runner_,
            [this](fidl::InterfaceRequest<fuchsia::sys::Runner> request) {
              received_requests_.push_back(std::move(request));
              if (on_request_received_)
                on_request_received_.Run();
            },
            "fuchsia.sys.Runner-cast") {}

  void SetUp() override {
    cast_runner_services_ = StartCfv1Shim();

    // Connect to the CastRunner's fuchsia.sys.Runner interface.
    cast_runner_shim_ = cast_runner_services_->Connect<fuchsia::sys::Runner>();
    cast_runner_shim_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CastRunner closed channel.";
      ADD_FAILURE();
    });

    static constexpr base::StringPiece kTestServerRoot(
        "fuchsia_web/runners/cast/testdata");
    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    net::test_server::RegisterDefaultHandlers(&test_server_);
    ASSERT_TRUE(test_server_.Start());
  }

  void TearDown() override {
    // Unbind the Runner channel, to prevent it from triggering an error when
    // the CastRunner and WebEngine are torn down.
    cast_runner_shim_.Unbind();
  }

  void RunUntilRequestsReceived(size_t expected_count) {
    base::RunLoop run_loop;
    base::AutoReset reset(&on_request_received_,
                          base::BindLambdaForTesting([&]() {
                            if (received_requests_.size() == expected_count)
                              run_loop.Quit();
                          }));
    run_loop.Run();
  }

  ::fuchsia::sys::ComponentControllerPtr& controller_ptr() {
    return controller_.ptr();
  }
  ::fuchsia::sys::RunnerPtr& cast_runner_shim_ptr() {
    return cast_runner_shim_;
  }

  std::vector<fidl::InterfaceRequest<fuchsia::sys::Runner>> received_requests_;

 private:
  std::unique_ptr<sys::ServiceDirectory> StartCfv1Shim() {
    fuchsia::sys::LaunchInfo launch_info;
    launch_info.url =
        "fuchsia-pkg://fuchsia.com/cast_runner#meta/cast_runner.cmx";

    // Clone stderr from the current process to CastRunner and ask it to
    // redirect all logs to stderr.
    launch_info.err = fuchsia::sys::FileDescriptor::New();
    launch_info.err->type0 = PA_FD;
    zx_status_t status = fdio_fd_clone(
        STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
    ZX_CHECK(status == ZX_OK, status);

    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(kDisableVulkanForTestsSwitch);
    command_line.AppendSwitchASCII("enable-logging", "stderr");

    // Add all switches and arguments, skipping the program.
    launch_info.arguments.emplace(std::vector<std::string>(
        command_line.argv().begin() + 1, command_line.argv().end()));

    auto additional_services = std::make_unique<fuchsia::sys::ServiceList>();
    auto* svc_dir = services_for_runner_.GetOrCreateDirectory("svc");
    additional_services->names.push_back("fuchsia.sys.Runner-cast");

    fuchsia::io::DirectoryHandle svc_dir_handle;
    svc_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                       fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                   svc_dir_handle.NewRequest().TakeChannel());
    additional_services->host_directory = std::move(svc_dir_handle);

    launch_info.additional_services = std::move(additional_services);

    fuchsia::io::DirectoryHandle cast_runner_services_dir;
    launch_info.directory_request = cast_runner_services_dir.NewRequest();

    fuchsia::sys::LauncherPtr launcher;
    base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
    launcher->CreateComponent(std::move(launch_info),
                              controller_.ptr().NewRequest());
    return std::make_unique<sys::ServiceDirectory>(
        std::move(cast_runner_services_dir));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  net::EmbeddedTestServer test_server_;

  // TODO(https://crbug.com/1168538): Override the RunLoop timeout set by
  // |task_environment_| to allow for the very high variability in web.Context
  // launch times.
  const base::test::ScopedRunLoopTimeout scoped_timeout_{
      FROM_HERE, TestTimeouts::action_max_timeout()};

  std::unique_ptr<sys::ServiceDirectory> cast_runner_services_;
  fuchsia::sys::RunnerPtr cast_runner_shim_;
  base::TestComponentController controller_;

  // Directory used to publish test ContextProvider to CastRunner.
  sys::OutgoingDirectory services_for_runner_;
  base::ScopedServicePublisher<fuchsia::sys::Runner> fake_runner_publisher_;
  base::RepeatingClosure on_request_received_;
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
}

// Ensure that CFv1 "shim" mode tears down the Runner component if the
// underlying service capability disconnects it. This is required in order to
// have the shim correctly reflect instability in the real Runner, to the CFv1
// framework.
TEST_F(CastRunnerCfv1ShimIntegrationTest, ExitOnFailure) {
  // |cast_runner_shim_| is expected to disconnect, so remove the error handler.
  cast_runner_shim_ptr().set_error_handler([](zx_status_t) {});

  // Wait for the two incoming Runner connections.
  RunUntilRequestsReceived(2u);

  // Close the two connections, and expect the Runner to self-terminate.
  received_requests_.clear();
  base::RunLoop loop;
  controller_ptr().set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  loop.Run();
}

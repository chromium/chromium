// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl_test_base.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_provider_impl.h"
#include "base/process/process.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

class WebRunnerSmokeTest : public testing::Test {
 public:
  WebRunnerSmokeTest() = default;
  WebRunnerSmokeTest(const WebRunnerSmokeTest&) = delete;
  WebRunnerSmokeTest& operator=(const WebRunnerSmokeTest&) = delete;

  void SetUp() final {
    // TODO(crbug.com/1309100) Update WebRunner to support headless mode.
    if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            "ozone-platform") == "headless") {
      GTEST_SKIP() << "Headless mode is not supported in WebRunner. "
                      "Skipping the test.";
    }

    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebRunnerSmokeTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
    service_provider_ = base::ServiceProviderImpl::CreateForOutgoingDirectory(
        &outgoing_directory_);
  }

  fuchsia::sys::LaunchInfo LaunchInfoWithServices() {
    auto services = fuchsia::sys::ServiceList::New();
    service_provider_->AddBinding(services->provider.NewRequest());
    fuchsia::sys::LaunchInfo launch_info;
    launch_info.additional_services = std::move(services);
    return launch_info;
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL absolute_url = test_server_.GetURL(request.relative_url);
    if (absolute_url.path() == "/test.html") {
      EXPECT_FALSE(test_html_requested_);
      test_html_requested_ = true;
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content("<!doctype html><img src=\"/img.png\">");
      http_response->set_content_type("text/html");
      return http_response;
    } else if (absolute_url.path() == "/window_close.html") {
      auto http_response =
          std::make_unique<net::test_server::BasicHttpResponse>();
      http_response->set_code(net::HTTP_OK);
      http_response->set_content(
          "<!doctype html><script>window.close();</script>");
      http_response->set_content_type("text/html");
      return http_response;
    } else if (absolute_url.path() == "/img.png") {
      EXPECT_FALSE(test_image_requested_);
      test_image_requested_ = true;
      // All done!
      run_loop_.Quit();
    }
    return nullptr;
  }

 protected:
  // Returns a fuchsia.sys.Launcher to be used when launching web_runner. The
  // returned instance belongs to a fuchsia.sys.Environment that has access to
  // all services available to this test component. This is necessary because
  // the default Launcher available to tests run by the Fuchsia test_manager
  // does not have access to system services.
  fuchsia::sys::Launcher* GetLauncher() {
    if (runner_environment_launcher_)
      return runner_environment_launcher_.get();

    // Collect the names of all services provided to the test. Calling stat() in
    // /svc is problematic; see https://fxbug.dev/100207. Tell the enumerator
    // not to recurse, to return both files and directories, and to report only
    // the names of entries.
    std::vector<std::string> runner_services;
    base::FileEnumerator file_enum(base::FilePath("/svc"), /*recursive=*/false,
                                   base::FileEnumerator::NAMES_ONLY);
    for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
      runner_services.push_back(file.BaseName().value());
    }

    auto environment = base::ComponentContextForProcess()
                           ->svc()
                           ->Connect<fuchsia::sys::Environment>();

    // Provide all of this test component's services to the runner.
    auto services = fuchsia::sys::ServiceList::New();
    services->names = std::move(runner_services);
    services->host_directory =
        base::ComponentContextForProcess()->svc()->CloneChannel();

    fuchsia::sys::EnvironmentPtr runner_environment;
    environment->CreateNestedEnvironment(
        runner_environment.NewRequest(),
        runner_environment_controller_.NewRequest(),
        base::StringPrintf("web_runners:%lu", base::Process::Current().Pid()),
        std::move(services),
        {.inherit_parent_services = false,
         .use_parent_runners = false,
         .delete_storage_on_death = true});

    runner_environment->GetLauncher(runner_environment_launcher_.NewRequest());
    runner_environment_launcher_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Launcher disconnected.";
    });
    runner_environment_controller_.set_error_handler([](zx_status_t status) {
      ZX_LOG(ERROR, status) << "EnvironmentController disconnected.";
    });

    return runner_environment_launcher_.get();
  }

  bool test_html_requested_ = false;
  bool test_image_requested_ = false;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  sys::OutgoingDirectory outgoing_directory_;
  fuchsia::sys::EnvironmentControllerPtr runner_environment_controller_;
  fuchsia::sys::LauncherPtr runner_environment_launcher_;
  std::unique_ptr<base::ServiceProviderImpl> service_provider_;

  net::EmbeddedTestServer test_server_;

  base::RunLoop run_loop_;
};

// Verify that the Component loads and fetches the desired page.
TEST_F(WebRunnerSmokeTest, RequestHtmlAndImage) {
  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/test.html").spec();

  fuchsia::sys::ComponentControllerSyncPtr controller;
  GetLauncher()->CreateComponent(std::move(launch_info),
                                 controller.NewRequest());

  run_loop_.Run();

  EXPECT_TRUE(test_html_requested_);
  EXPECT_TRUE(test_image_requested_);
}

// Verify that the Component can be terminated via the Lifecycle API.
TEST_F(WebRunnerSmokeTest, LifecycleTerminate) {
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;

  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/test.html").spec();
  launch_info.directory_request = directory.NewRequest();

  fuchsia::sys::ComponentControllerPtr controller;
  GetLauncher()->CreateComponent(std::move(launch_info),
                                 controller.NewRequest());

  sys::ServiceDirectory component_services(std::move(directory));
  auto lifecycle = component_services.Connect<fuchsia::modular::Lifecycle>();
  ASSERT_TRUE(lifecycle);

  // Terminate() the component, and expect that |controller| disconnects us.
  base::RunLoop loop;
  controller.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  lifecycle->Terminate();
  loop.Run();

  EXPECT_FALSE(controller);
}

// Verify that if the Frame disconnects, the Component tears down.
TEST_F(WebRunnerSmokeTest, ComponentExitOnFrameClose) {
  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/window_close.html").spec();

  fuchsia::sys::ComponentControllerPtr controller;
  GetLauncher()->CreateComponent(std::move(launch_info),
                                 controller.NewRequest());

  // Script in the page will execute window.close(), which should teardown the
  // Component, causing |controller| to be disconnected.
  base::RunLoop loop;
  controller.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  loop.Run();

  EXPECT_FALSE(controller);
}

class MockModuleContext
    : public fuchsia::modular::testing::ModuleContext_TestBase {
 public:
  MockModuleContext() = default;

  MockModuleContext(const MockModuleContext&) = delete;
  MockModuleContext& operator=(const MockModuleContext&) = delete;

  ~MockModuleContext() override = default;

  MOCK_METHOD0(RemoveSelfFromStory, void());

  void NotImplemented_(const std::string& name) override {
    NOTIMPLEMENTED() << name;
  }
};

// Verify that Modular's RemoveSelfFromStory() is called on teardown.
TEST_F(WebRunnerSmokeTest, RemoveSelfFromStoryOnFrameClose) {
  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/window_close.html").spec();

  MockModuleContext module_context;
  EXPECT_CALL(module_context, RemoveSelfFromStory);
  base::ScopedServiceBinding<fuchsia::modular::ModuleContext> binding(
      &outgoing_directory_, &module_context);
  launch_info.additional_services->names.emplace_back(
      fuchsia::modular::ModuleContext::Name_);

  fuchsia::sys::ComponentControllerPtr controller;
  GetLauncher()->CreateComponent(std::move(launch_info),
                                 controller.NewRequest());

  // Script in the page will execute window.close(), which should teardown the
  // Component, causing |controller| to be disconnected.
  base::RunLoop loop;
  controller.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        quit_loop.Run();
      });
  loop.Run();

  EXPECT_FALSE(controller);

  // Spin the loop again to ensure that RemoveSelfFromStory is processed.
  base::RunLoop().RunUntilIdle();
}

}  // anonymous namespace

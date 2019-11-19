// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/modular/cpp/fidl_test_base.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_provider_impl.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
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
  WebRunnerSmokeTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {}
  void SetUp() final {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &WebRunnerSmokeTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());

    fidl::InterfaceHandle<fuchsia::io::Directory> directory;
    outgoing_directory_.GetOrCreateDirectory("svc")->Serve(
        fuchsia::io::OPEN_RIGHT_READABLE | fuchsia::io::OPEN_RIGHT_WRITABLE,
        directory.NewRequest().TakeChannel());

    service_provider_ = std::make_unique<base::fuchsia::ServiceProviderImpl>(
        std::move(directory));
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
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  bool test_html_requested_ = false;
  bool test_image_requested_ = false;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  sys::OutgoingDirectory outgoing_directory_;
  std::unique_ptr<base::fuchsia::ServiceProviderImpl> service_provider_;

  net::EmbeddedTestServer test_server_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebRunnerSmokeTest);
};

// Verify that the Component loads and fetches the desired page.
TEST_F(WebRunnerSmokeTest, RequestHtmlAndImage) {
  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/test.html").spec();

  auto launcher = base::fuchsia::ComponentContextForCurrentProcess()
                      ->svc()
                      ->Connect<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerSyncPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

  run_loop_.Run();

  EXPECT_TRUE(test_html_requested_);
  EXPECT_TRUE(test_image_requested_);
}

// Verify that the Component can be terminated via the Lifecycle API.
TEST_F(WebRunnerSmokeTest, LifecycleTerminate) {
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;

  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/test.html").spec();
  launch_info.directory_request = directory.NewRequest().TakeChannel();

  auto launcher = base::fuchsia::ComponentContextForCurrentProcess()
                      ->svc()
                      ->Connect<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

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

  auto launcher = base::fuchsia::ComponentContextForCurrentProcess()
                      ->svc()
                      ->Connect<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

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
  ~MockModuleContext() override = default;

  MOCK_METHOD0(RemoveSelfFromStory, void());

  void NotImplemented_(const std::string& name) override {
    NOTIMPLEMENTED() << name;
  }

  DISALLOW_COPY_AND_ASSIGN(MockModuleContext);
};

// Verify that Modular's RemoveSelfFromStory() is called on teardown.
TEST_F(WebRunnerSmokeTest, RemoveSelfFromStoryOnFrameClose) {
  fuchsia::sys::LaunchInfo launch_info = LaunchInfoWithServices();
  launch_info.url = test_server_.GetURL("/window_close.html").spec();

  MockModuleContext module_context;
  EXPECT_CALL(module_context, RemoveSelfFromStory);
  base::fuchsia::ScopedServiceBinding<fuchsia::modular::ModuleContext> binding(
      &outgoing_directory_, &module_context);
  launch_info.additional_services->names.emplace_back(
      fuchsia::modular::ModuleContext::Name_);

  auto launcher = base::fuchsia::ComponentContextForCurrentProcess()
                      ->svc()
                      ->Connect<fuchsia::sys::Launcher>();

  fuchsia::sys::ComponentControllerPtr controller;
  launcher->CreateComponent(std::move(launch_info), controller.NewRequest());

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

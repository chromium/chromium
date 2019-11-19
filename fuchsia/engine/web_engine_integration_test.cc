// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_devtools_list_fetcher.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kValidUserAgentProduct[] = "TestProduct";
constexpr char kValidUserAgentVersion[] = "dev.12345";
constexpr char kValidUserAgentProductAndVersion[] = "TestProduct/dev.12345";
constexpr char kInvalidUserAgentProduct[] = "Test/Product";
constexpr char kInvalidUserAgentVersion[] = "dev/12345";

}  // namespace

class WebEngineIntegrationTest : public testing::Test {
 public:
  WebEngineIntegrationTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~WebEngineIntegrationTest() override = default;

  void SetUp() override {
    web_context_provider_ = base::fuchsia::ComponentContextForCurrentProcess()
                                ->svc()
                                ->Connect<fuchsia::web::ContextProvider>();
    web_context_provider_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });

    net::test_server::RegisterDefaultHandlers(&embedded_test_server_);
    ASSERT_TRUE(embedded_test_server_.Start());
  }

  fuchsia::web::CreateContextParams DefaultContextParams() const {
    fuchsia::web::CreateContextParams create_params;
    auto directory = base::fuchsia::OpenDirectory(
        base::FilePath(base::fuchsia::kServiceDirectoryPath));
    EXPECT_TRUE(directory.is_valid());
    create_params.set_service_directory(std::move(directory));
    return create_params;
  }

  void CreateContextAndFrame(fuchsia::web::CreateContextParams params) {
    web_context_provider_->Create(std::move(params), context_.NewRequest());
    context_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

    context_->CreateFrame(frame_.NewRequest());
    frame_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

    frame_->GetNavigationController(navigation_controller_.NewRequest());
    navigation_controller_.set_error_handler(
        [](zx_status_t status) { ADD_FAILURE(); });
  }

  void CreateContextAndExpectError(fuchsia::web::CreateContextParams params,
                                   zx_status_t expected_error) {
    web_context_provider_->Create(std::move(params), context_.NewRequest());
    base::RunLoop run_loop;
    context_.set_error_handler([&run_loop, expected_error](zx_status_t status) {
      EXPECT_EQ(status, expected_error);
      run_loop.Quit();
    });
    run_loop.Run();
  }

  void CreateContextAndFrameAndLoadUrl(fuchsia::web::CreateContextParams params,
                                       const GURL& url) {
    CreateContextAndFrame(std::move(params));

    // Attach a navigation listener, to monitor the state of the Frame.
    cr_fuchsia::TestNavigationListener listener;
    fidl::Binding<fuchsia::web::NavigationEventListener> listener_binding(
        &listener);
    frame_->SetNavigationEventListener(listener_binding.NewBinding());

    // Navigate the Frame to |url| and wait for it to complete loading.
    fuchsia::web::LoadUrlParams load_url_params;
    ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        navigation_controller_.get(), std::move(load_url_params), url.spec()));

    // Wait for the URL to finish loading.
    listener.RunUntilUrlEquals(url);
  }

  std::string ExecuteJavaScriptWithStringResult(base::StringPiece script) {
    base::Optional<base::Value> value =
        cr_fuchsia::ExecuteJavaScript(frame_.get(), script);
    return value ? value->GetString() : std::string();
  }

 protected:
  const base::test::TaskEnvironment task_environment_;

  fuchsia::web::ContextProviderPtr web_context_provider_;

  net::EmbeddedTestServer embedded_test_server_;

  fuchsia::web::ContextPtr context_;
  fuchsia::web::FramePtr frame_;
  fuchsia::web::NavigationControllerPtr navigation_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineIntegrationTest);
};

TEST_F(WebEngineIntegrationTest, ValidUserAgent) {
  const std::string kEchoHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  const GURL kEchoUserAgentUrl(embedded_test_server_.GetURL(kEchoHeaderPath));

  {
    // Create a Context with just an embedder product specified.
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();
    create_params.set_user_agent_product(kValidUserAgentProduct);
    CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                    kEchoUserAgentUrl);

    // Query & verify that the header echoed into the document body contains
    // the product tag.
    std::string result =
        ExecuteJavaScriptWithStringResult("document.body.innerText;");
    EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);

    // Query & verify that the navigator.userAgent contains the product tag.
    result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
    EXPECT_TRUE(result.find(kValidUserAgentProduct) != std::string::npos);
  }

  {
    // Create a Context with both product and version specified.
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();
    create_params.set_user_agent_product(kValidUserAgentProduct);
    create_params.set_user_agent_version(kValidUserAgentVersion);
    CreateContextAndFrameAndLoadUrl(std::move(create_params),
                                    kEchoUserAgentUrl);

    // Query & verify that the header echoed into the document body contains
    // both product & version.
    std::string result =
        ExecuteJavaScriptWithStringResult("document.body.innerText;");
    EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
                std::string::npos);

    // Query & verify that the navigator.userAgent contains product & version.
    result = ExecuteJavaScriptWithStringResult("navigator.userAgent;");
    EXPECT_TRUE(result.find(kValidUserAgentProductAndVersion) !=
                std::string::npos);
  }
}

TEST_F(WebEngineIntegrationTest, InvalidUserAgent) {
  const std::string kEchoHeaderPath =
      std::string("/echoheader?") + net::HttpRequestHeaders::kUserAgent;
  const GURL kEchoUserAgentUrl(embedded_test_server_.GetURL(kEchoHeaderPath));

  {
    // Try to create a Context with an invalid embedder product tag.
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();
    create_params.set_user_agent_product(kInvalidUserAgentProduct);
    CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
  }

  {
    // Try to create a Context with an embedder version but no product.
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();
    create_params.set_user_agent_version(kValidUserAgentVersion);
    CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
  }

  {
    // Try to create a Context with valid product tag, but invalid version.
    fuchsia::web::CreateContextParams create_params = DefaultContextParams();
    create_params.set_user_agent_product(kValidUserAgentProduct);
    create_params.set_user_agent_version(kInvalidUserAgentVersion);
    CreateContextAndExpectError(std::move(create_params), ZX_ERR_INVALID_ARGS);
  }
}

// Check that if the CreateContextParams has |remote_debugging_port| set then:
// - DevTools becomes available when the first debuggable Frame is created.
// - DevTools closes when the last debuggable Frame is closed.
TEST_F(WebEngineIntegrationTest, RemoteDebuggingPort) {
  // Create a Context with remote debugging enabled via an ephemeral port.
  fuchsia::web::CreateContextParams create_params;
  auto directory = base::fuchsia::OpenDirectory(
      base::FilePath(base::fuchsia::kServiceDirectoryPath));
  ASSERT_TRUE(directory.is_valid());
  create_params.set_service_directory(std::move(directory));
  create_params.set_remote_debugging_port(0);

  fuchsia::web::ContextPtr web_context;
  web_context_provider_->Create(std::move(create_params),
                                web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  // Create a Frame with remote debugging enabled.
  fuchsia::web::FramePtr web_frame;
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  web_context->CreateFrameWithParams(std::move(create_frame_params),
                                     web_frame.NewRequest());
  web_frame.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  // Expect to receive a notification of the selected DevTools port.
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<
      fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver(run_loop.QuitClosure());
  web_context->GetRemoteDebuggingPort(
      cr_fuchsia::CallbackToFitFunction(port_receiver.GetReceiveCallback()));
  run_loop.Run();

  ASSERT_TRUE(port_receiver->is_response());
  uint16_t remote_debugging_port = port_receiver->response().port;
  ASSERT_TRUE(remote_debugging_port != 0);

  // Navigate the Frame to a URL, and verify the URL via DevTools.
  fuchsia::web::NavigationControllerPtr nav_controller;
  web_frame->GetNavigationController(nav_controller.NewRequest());

  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener> listener_binding(
      &navigation_listener);
  web_frame->SetNavigationEventListener(listener_binding.NewBinding());

  // Navigate to a URL.
  GURL url = embedded_test_server_.GetURL("/defaultresponse");
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      nav_controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener.RunUntilUrlEquals(url);

  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  base::Value* devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Create a second frame, without remote debugging enabled. The remote
  // debugging service should still report a single Frame is present.
  fuchsia::web::FramePtr web_frame2;
  web_context->CreateFrame(web_frame2.NewRequest());
  web_frame2.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetList().size(), 1u);

  devtools_url = devtools_list.GetList()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Tear down the debuggable Frame. The remote debugging service should have
  // shut down.
  base::RunLoop controller_run_loop;
  nav_controller.set_error_handler(
      [&controller_run_loop](zx_status_t) { controller_run_loop.Quit(); });
  web_frame.Unbind();

  // Wait until the NavigationController shuts down to ensure WebEngine has
  // handled the Frame tear down.
  controller_run_loop.Run();

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  EXPECT_TRUE(devtools_list.is_none());
}

// Check that remote debugging requests for Frames in non-debuggable Contexts
// cause an error to be reported.
TEST_F(WebEngineIntegrationTest, RequestDebuggableFrameInNonDebuggableContext) {
  fuchsia::web::CreateContextParams create_params = DefaultContextParams();

  fuchsia::web::ContextPtr web_context;
  web_context_provider_->Create(std::move(create_params),
                                web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

  fuchsia::web::FramePtr web_frame;
  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  web_context->CreateFrameWithParams(std::move(create_frame_params),
                                     web_frame.NewRequest());

  base::RunLoop loop;
  web_frame.set_error_handler(
      [quit_loop = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
        quit_loop.Run();
      });
  loop.Run();
}

// Navigates to a resource served under the "testdata" ContentDirectory.
TEST_F(WebEngineIntegrationTest, ContentDirectoryProvider) {
  const GURL kUrl("fuchsia-dir://testdata/title1.html");
  constexpr char kTitle[] = "title 1";

  fuchsia::web::CreateContextParams create_params = DefaultContextParams();

  fuchsia::web::ContentDirectoryProvider provider;
  provider.set_name("testdata");
  base::FilePath pkg_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &pkg_path));
  provider.set_directory(base::fuchsia::OpenDirectory(
      pkg_path.AppendASCII("fuchsia/engine/test/data")));

  create_params.mutable_content_directories()->emplace_back(
      std::move(provider));

  CreateContextAndFrame(std::move(create_params));

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());
  controller.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });

  // Navigate to test1.html and verify that the resource was correctly
  // downloaded and interpreted by inspecting the document title.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener> listener_binding(
      &navigation_listener);
  frame_->SetNavigationEventListener(listener_binding.NewBinding());
  navigation_listener.RunUntilUrlAndTitleEquals(kUrl, kTitle);
}

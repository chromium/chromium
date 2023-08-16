// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>

#include "base/containers/contains.h"
#include "base/fuchsia/file_utils.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_debug_listener.h"
#include "fuchsia_web/common/test/test_devtools_list_fetcher.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/test/context_provider_for_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestServerRoot[] = "fuchsia_web/webengine/test/data";

}  // namespace

class WebEngineDebugIntegrationTest : public testing::Test {
 public:
  WebEngineDebugIntegrationTest()
      : web_context_provider_(ContextProviderForDebugTest::Create(
            base::CommandLine(base::CommandLine::NO_PROGRAM))),
        dev_tools_listener_binding_(&dev_tools_listener_) {
    web_context_provider_.ptr().set_error_handler(
        [](zx_status_t status) { FAIL() << zx_status_get_string(status); });
  }

  WebEngineDebugIntegrationTest(const WebEngineDebugIntegrationTest&) = delete;
  WebEngineDebugIntegrationTest& operator=(
      const WebEngineDebugIntegrationTest&) = delete;

  ~WebEngineDebugIntegrationTest() override = default;

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        web_context_provider_.ConnectToDebug(debug_.NewRequest()));

    // Attach the DevToolsListener. EnableDevTools has an acknowledgement
    // callback so the listener will have been added after this call returns.
    debug_->EnableDevTools(dev_tools_listener_binding_.NewBinding());

    test_server_.ServeFilesFromSourceDirectory(kTestServerRoot);
    ASSERT_TRUE(test_server_.Start());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  ContextProviderForDebugTest web_context_provider_;
  TestDebugListener dev_tools_listener_;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding_;
  fuchsia::web::DebugSyncPtr debug_;

  base::OnceClosure on_url_fetch_complete_ack_;

  net::EmbeddedTestServer test_server_;
};

enum class UserModeDebugging { kEnabled = 0, kDisabled = 1 };

// Helper struct to intiialize all data necessary for a Context to create a
// Frame and navigate it to a specific URL.
struct TestContextAndFrame {
  explicit TestContextAndFrame(fuchsia::web::ContextProvider* context_provider,
                               UserModeDebugging user_mode_debugging,
                               std::string url) {
    // Create a Context, a Frame and navigate it to |url|.
    auto directory =
        base::OpenDirectoryHandle(base::FilePath(base::kServiceDirectoryPath));
    if (!directory.is_valid())
      return;

    fuchsia::web::CreateContextParams create_params;
    create_params.set_features(fuchsia::web::ContextFeatureFlags::NETWORK);
    create_params.set_service_directory(std::move(directory));
    if (user_mode_debugging == UserModeDebugging::kEnabled)
      create_params.set_remote_debugging_port(0);
    context_provider->Create(std::move(create_params), context.NewRequest());
    context->CreateFrame(frame.NewRequest());
    frame->GetNavigationController(controller.NewRequest());
    if (!LoadUrlAndExpectResponse(controller.get(),
                                  fuchsia::web::LoadUrlParams(), url)) {
      ADD_FAILURE();
      context.Unbind();
      frame.Unbind();
      controller.Unbind();
      return;
    }
  }

  TestContextAndFrame(const TestContextAndFrame&) = delete;
  TestContextAndFrame& operator=(const TestContextAndFrame&) = delete;

  ~TestContextAndFrame() = default;

  fuchsia::web::ContextPtr context;
  fuchsia::web::FramePtr frame;
  fuchsia::web::NavigationControllerPtr controller;
};

// Test the Debug service is properly started and accessible.
TEST_F(WebEngineDebugIntegrationTest, DebugService) {
  std::string url = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data(web_context_provider_.get(),
                                 UserModeDebugging::kDisabled, url);
  ASSERT_TRUE(frame_data.context);

  // Test the debug information is correct.
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(1u));

  base::Value::List devtools_list =
      GetDevToolsListFromPort(*dev_tools_listener_.debug_ports().begin());
  EXPECT_EQ(devtools_list.size(), 1u);

  const auto& devtools_dict = devtools_list[0].GetDict();
  const auto* devtools_url = devtools_dict.FindString("url");
  ASSERT_TRUE(devtools_url);
  EXPECT_EQ(*devtools_url, url);

  const auto* devtools_title = devtools_dict.FindString("title");
  ASSERT_TRUE(devtools_title);
  EXPECT_EQ(*devtools_title, "title 1");

  // Unbind the context and wait for the listener to no longer have any active
  // DevTools port.
  frame_data.context.Unbind();
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(0));
}

TEST_F(WebEngineDebugIntegrationTest, MultipleDebugClients) {
  std::string url1 = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data1(web_context_provider_.get(),
                                  UserModeDebugging::kDisabled, url1);
  ASSERT_TRUE(frame_data1.context);

  // Test the debug information is correct.
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(1u));
  uint16_t port1 = *dev_tools_listener_.debug_ports().begin();

  base::Value::List devtools_list1 = GetDevToolsListFromPort(port1);
  EXPECT_EQ(devtools_list1.size(), 1u);

  const auto& devtools_dict1 = devtools_list1[0].GetDict();
  const auto* devtools_url1 = devtools_dict1.FindString("url");
  ASSERT_TRUE(devtools_url1);
  EXPECT_EQ(*devtools_url1, url1);

  const auto* devtools_title1 = devtools_dict1.FindString("title");
  ASSERT_TRUE(devtools_title1);
  EXPECT_EQ(*devtools_title1, "title 1");

  // Connect a second Debug interface.
  fuchsia::web::DebugSyncPtr debug2;
  ASSERT_NO_FATAL_FAILURE(
      web_context_provider_.ConnectToDebug(debug2.NewRequest()));
  TestDebugListener dev_tools_listener2;
  fidl::Binding<fuchsia::web::DevToolsListener> dev_tools_listener_binding2(
      &dev_tools_listener2);
  debug2->EnableDevTools(dev_tools_listener_binding2.NewBinding());

  // Create a second Context, a second Frame and navigate it to title2.html.
  std::string url2 = test_server_.GetURL("/title2.html").spec();
  TestContextAndFrame frame_data2(web_context_provider_.get(),
                                  UserModeDebugging::kDisabled, url2);
  ASSERT_TRUE(frame_data2.context);

  // Ensure each DevTools listener has the right information.
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(2u));
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener2.RunUntilNumberOfPortsIs(1u));

  uint16_t port2 = *dev_tools_listener2.debug_ports().begin();
  ASSERT_NE(port1, port2);
  ASSERT_TRUE(base::Contains(dev_tools_listener_.debug_ports(), port2));

  base::Value::List devtools_list2 = GetDevToolsListFromPort(port2);
  EXPECT_EQ(devtools_list2.size(), 1u);

  const auto& devtools_dict2 = devtools_list2[0].GetDict();
  const auto* devtools_url2 = devtools_dict2.FindString("url");
  ASSERT_TRUE(devtools_url2);
  EXPECT_EQ(*devtools_url2, url2);

  const auto* devtools_title2 = devtools_dict2.FindString("title");
  ASSERT_TRUE(devtools_title2);
  EXPECT_EQ(*devtools_title2, "title 2");

  // Unbind the first Context, each listener should still have one open port.
  frame_data1.context.Unbind();
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(1u));
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener2.RunUntilNumberOfPortsIs(1u));

  // Unbind the second Context, no listener should have any open port.
  frame_data2.context.Unbind();
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(0));
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener2.RunUntilNumberOfPortsIs(0));
}

// Test the Debug service is accessible when the User service is requested.
TEST_F(WebEngineDebugIntegrationTest, DebugAndUserService) {
  std::string url = test_server_.GetURL("/title1.html").spec();
  TestContextAndFrame frame_data(web_context_provider_.get(),
                                 UserModeDebugging::kEnabled, url);
  ASSERT_TRUE(frame_data.context);

  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(1u));

  // Check we are getting the same port on both the debug and user APIs.
  base::test::TestFuture<fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver;
  frame_data.context->GetRemoteDebuggingPort(
      CallbackToFitFunction(port_receiver.GetCallback()));
  ASSERT_TRUE(port_receiver.Wait());

  ASSERT_TRUE(port_receiver.Get().is_response());
  uint16_t remote_debugging_port = port_receiver.Get().response().port;
  ASSERT_EQ(remote_debugging_port, *dev_tools_listener_.debug_ports().begin());

  // Test the debug information is correct.
  base::Value::List devtools_list =
      GetDevToolsListFromPort(remote_debugging_port);
  EXPECT_EQ(devtools_list.size(), 1u);

  const auto& devtools_dict = devtools_list[0].GetDict();
  const auto* devtools_url = devtools_dict.FindString("url");
  ASSERT_TRUE(devtools_url);
  EXPECT_EQ(*devtools_url, url);

  const auto* devtools_title = devtools_dict.FindString("title");
  ASSERT_TRUE(devtools_title);
  EXPECT_EQ(*devtools_title, "title 1");

  // Unbind the context and wait for the listener to no longer have any active
  // DevTools port.
  frame_data.context.Unbind();
  ASSERT_NO_FATAL_FAILURE(dev_tools_listener_.RunUntilNumberOfPortsIs(0));
}

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "fuchsia/base/test/fit_adapter.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_devtools_list_fetcher.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/test/frame_for_test.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class WebInstanceHostIntegrationTest : public testing::Test {
 public:
  WebInstanceHostIntegrationTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        filtered_service_directory_(base::ComponentContextForProcess()->svc()) {
    // Push all services from /svc to the filtered service directory.
    base::FileEnumerator file_enum(base::FilePath("/svc"), false,
                                   base::FileEnumerator::FILES);
    for (auto file = file_enum.Next(); !file.empty(); file = file_enum.Next()) {
      zx_status_t status = filtered_service_directory_.AddService(
          file_enum.GetInfo().GetName().value().c_str());
      ZX_CHECK(status == ZX_OK, status)
          << "FilteredServiceDirectory::AddService";
    }
  }

  ~WebInstanceHostIntegrationTest() override = default;
  WebInstanceHostIntegrationTest(const WebInstanceHostIntegrationTest&) =
      delete;
  WebInstanceHostIntegrationTest& operator=(
      const WebInstanceHostIntegrationTest&) = delete;

  void SetUp() override {
    // Override command line. This is necessary to get WebEngine logs.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII("enable-logging", "stderr");

    embedded_test_server_.ServeFilesFromSourceDirectory(
        "fuchsia_web/webengine/test/data");
    net::test_server::RegisterDefaultHandlers(&embedded_test_server_);
    ASSERT_TRUE(embedded_test_server_.Start());
  }

 protected:
  fuchsia::web::CreateContextParams TestContextParams() {
    fuchsia::web::CreateContextParams create_params;
    create_params.set_features(fuchsia::web::ContextFeatureFlags::NETWORK);
    zx_status_t status = filtered_service_directory_.ConnectClient(
        create_params.mutable_service_directory()->NewRequest());
    ZX_CHECK(status == ZX_OK, status)
        << "FilteredServiceDirectory::ConnectClient";
    return create_params;
  }

  void CreateContext(fuchsia::web::CreateContextParams context_params) {
    EXPECT_FALSE(context_);

    fuchsia::io::DirectoryHandle web_instance_services;
    web_instance_host_.CreateInstanceForContextWithCopiedArgs(
        std::move(context_params), web_instance_services.NewRequest(),
        *base::CommandLine::ForCurrentProcess());
    web_instance_services_ = std::make_unique<sys::ServiceDirectory>(
        std::move(web_instance_services));

    zx_status_t result = web_instance_services_->Connect(context_.NewRequest());
    ZX_CHECK(result == ZX_OK, result) << "fdio_service_connect_at";
    context_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
  }

  void ConnectFrameHost() {
    zx_status_t result =
        web_instance_services_->Connect(frame_host_.NewRequest());
    ZX_CHECK(result == ZX_OK, result) << "fdio_service_connect_at";
    frame_host_.set_error_handler([](zx_status_t status) { ADD_FAILURE(); });
  }

  const base::test::TaskEnvironment task_environment_;

  fidl::InterfaceHandle<fuchsia::sys::ComponentController>
      web_engine_controller_;

  cr_fuchsia::WebInstanceHost web_instance_host_;
  std::unique_ptr<sys::ServiceDirectory> web_instance_services_;
  fuchsia::web::ContextPtr context_;
  fuchsia::web::FrameHostPtr frame_host_;

  net::EmbeddedTestServer embedded_test_server_;

  base::FilteredServiceDirectory filtered_service_directory_;
};

// Check that connecting and disconnecting to the FrameHost service does not
// trigger shutdown of the devtools service.
TEST_F(WebInstanceHostIntegrationTest, FrameHostDebugging) {
  fuchsia::web::CreateContextParams create_params = TestContextParams();
  create_params.set_remote_debugging_port(0);
  CreateContext(std::move(create_params));

  fuchsia::web::CreateFrameParams create_frame_params;
  create_frame_params.set_enable_remote_debugging(true);
  auto frame = cr_fuchsia::FrameForTest::Create(context_,
                                                std::move(create_frame_params));

  // Expect to receive a notification of the selected DevTools port.
  base::test::TestFuture<fuchsia::web::Context_GetRemoteDebuggingPort_Result>
      port_receiver;
  context_->GetRemoteDebuggingPort(
      cr_fuchsia::CallbackToFitFunction(port_receiver.GetCallback()));
  ASSERT_TRUE(port_receiver.Wait());
  ASSERT_TRUE(port_receiver.Get().is_response());
  uint16_t remote_debugging_port = port_receiver.Get().response().port;
  ASSERT_TRUE(remote_debugging_port != 0);

  // Navigate to a URL, the devtools service should be active and report a
  // single page.
  GURL url = embedded_test_server_.GetURL("/defaultresponse");
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url);
  base::Value devtools_list =
      cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetListDeprecated().size(), 1u);
  base::Value* devtools_url =
      devtools_list.GetListDeprecated()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url);

  // Connect to the FrameHost and immediately disconnect it.
  ConnectFrameHost();
  frame_host_.Unbind();

  // Navigate to a different page. The devtools service should still be active
  // and report the new page.
  GURL url2 = embedded_test_server_.GetURL("/title1.html");
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url2.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url2);

  devtools_list = cr_fuchsia::GetDevToolsListFromPort(remote_debugging_port);
  ASSERT_TRUE(devtools_list.is_list());
  EXPECT_EQ(devtools_list.GetListDeprecated().size(), 1u);
  devtools_url = devtools_list.GetListDeprecated()[0].FindPath("url");
  ASSERT_TRUE(devtools_url->is_string());
  EXPECT_EQ(devtools_url->GetString(), url2);
}

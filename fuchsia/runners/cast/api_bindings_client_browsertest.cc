// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/test/bind_test_util.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/api_bindings_client.h"
#include "fuchsia/runners/cast/test_api_bindings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ApiBindingsClientTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  ApiBindingsClientTest() : api_service_binding_(&api_service_) {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
  }

  ~ApiBindingsClientTest() override = default;

 protected:
  void StartClient() {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Get the bindings from |api_service_|.
    base::RunLoop run_loop;
    client_ = std::make_unique<ApiBindingsClient>(
        api_service_binding_.NewBinding(), run_loop.QuitClosure());
    ASSERT_FALSE(client_->HasBindings());
    run_loop.Run();
    ASSERT_TRUE(client_->HasBindings());

    frame_ = WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    frame_->GetNavigationController(controller_.NewRequest());
    connector_ = std::make_unique<NamedMessagePortConnector>(frame_.get());
    client_->AttachToFrame(frame_.get(), connector_.get(),
                           base::MakeExpectedNotRunClosure(FROM_HERE));
  }

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Destroy |client_| before the MessageLoop is destroyed.
    client_.reset();
  }

  fuchsia::web::FramePtr frame_;
  std::unique_ptr<NamedMessagePortConnector> connector_;
  TestApiBindings api_service_;
  fidl::Binding<chromium::cast::ApiBindings> api_service_binding_;
  std::unique_ptr<ApiBindingsClient> client_;
  cr_fuchsia::TestNavigationListener navigation_listener_;
  fuchsia::web::NavigationControllerPtr controller_;

  DISALLOW_COPY_AND_ASSIGN(ApiBindingsClientTest);
};

IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest, EndToEnd) {
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(cr_fuchsia::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('echoService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  api_service_.set_bindings(std::move(binding_list));
  StartClient();

  const GURL test_url = embedded_test_server()->GetURL("/echo.html");
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller_.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url);
  connector_->OnPageLoad();

  fuchsia::web::MessagePortPtr port =
      api_service_.RunUntilMessagePortReceived("echoService").Bind();

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString("ping", "ping-msg"));
  port->PostMessage(std::move(message),
                    [](fuchsia::web::MessagePort_PostMessage_Result result) {
                      EXPECT_TRUE(result.is_response());
                    });

  base::RunLoop response_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> response(
      response_loop.QuitClosure());
  port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(response.GetReceiveCallback()));
  response_loop.Run();

  std::string response_string;
  EXPECT_TRUE(
      cr_fuchsia::StringFromMemBuffer(response->data(), &response_string));
  EXPECT_EQ("ack ping", response_string);
}

}  // namespace

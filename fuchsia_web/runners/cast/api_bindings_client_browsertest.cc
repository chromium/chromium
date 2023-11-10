// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/cast/message_port/fuchsia/create_web_message.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/runners/cast/api_bindings_client.h"
#include "fuchsia_web/runners/cast/named_message_port_connector_fuchsia.h"
#include "fuchsia_web/runners/cast/test/fake_api_bindings.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ApiBindingsClientTest : public WebEngineBrowserTest {
 public:
  ApiBindingsClientTest() : api_service_binding_(&api_service_) {
    set_test_server_root(base::FilePath("fuchsia_web/runners/cast/testdata"));
  }

  ~ApiBindingsClientTest() override = default;

  ApiBindingsClientTest(const ApiBindingsClientTest&) = delete;
  ApiBindingsClientTest& operator=(const ApiBindingsClientTest&) = delete;

  void SetUp() override { WebEngineBrowserTest::SetUp(); }

 protected:
  void StartClient(bool disconnect_before_attach,
                   base::OnceClosure on_error_closure) {
    base::ScopedAllowBlockingForTesting allow_blocking;

    // Get the bindings from |api_service_|.
    base::RunLoop run_loop;
    client_ = std::make_unique<ApiBindingsClient>(
        api_service_binding_.NewBinding(), run_loop.QuitClosure());
    ASSERT_FALSE(client_->HasBindings());
    run_loop.Run();
    ASSERT_TRUE(client_->HasBindings());

    frame_ = FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
    connector_ =
        std::make_unique<NamedMessagePortConnectorFuchsia>(frame_.get());

    if (disconnect_before_attach)
      api_service_binding_.Unbind();

    base::RunLoop().RunUntilIdle();

    client_->AttachToFrame(frame_.get(), connector_.get(),
                           std::move(on_error_closure));
  }

  void SetUpOnMainThread() override {
    WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Destroy |client_| before the MessageLoop is destroyed.
    client_ = nullptr;

    // Destroy the NamedMessagePortConnector before the Frame.
    connector_ = nullptr;

    // Destroy the Frame before the test terminates
    frame_ = {};

    WebEngineBrowserTest::TearDownOnMainThread();
  }

  FrameForTest frame_;
  std::unique_ptr<NamedMessagePortConnectorFuchsia> connector_;
  FakeApiBindingsImpl api_service_;
  fidl::Binding<chromium::cast::ApiBindings> api_service_binding_;
  std::unique_ptr<ApiBindingsClient> client_;
};

// Tests API registration, injection, and message IPC.
// Registers a port that echoes messages received over a MessagePort back to the
// sender.
IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest, EndToEnd) {
  // Define the injected bindings.
  std::vector<chromium::cast::ApiBinding> binding_list;
  chromium::cast::ApiBinding echo_binding;
  echo_binding.set_before_load_script(base::MemBufferFromString(
      "window.echo = cast.__platform__.PortConnector.bind('echoService');",
      "test"));
  binding_list.emplace_back(std::move(echo_binding));
  api_service_.set_bindings(std::move(binding_list));

  StartClient(false, base::MakeExpectedNotRunClosure(FROM_HERE));

  base::RunLoop post_message_responses_loop;
  base::RepeatingClosure post_message_response_closure =
      base::BarrierClosure(2, post_message_responses_loop.QuitClosure());

  // Navigate to a test page that makes use of the injected bindings.
  const GURL test_url = embedded_test_server()->GetURL("/echo.html");
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       test_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(test_url);

  std::string connect_message;
  std::unique_ptr<cast_api_bindings::MessagePort> connect_port;
  connector_->GetConnectMessage(&connect_message, &connect_port);
  frame_->PostMessage(
      "*", CreateWebMessage(connect_message, std::move(connect_port)),
      [&post_message_response_closure](
          fuchsia::web::Frame_PostMessage_Result result) {
        ASSERT_TRUE(result.is_response());
        post_message_response_closure.Run();
      });

  // Connect to the echo service hosted by the page and send a ping to it.
  fuchsia::web::WebMessage message;
  message.set_data(base::MemBufferFromString("ping", "ping-msg"));
  fuchsia::web::MessagePortPtr port =
      api_service_.RunAndReturnConnectedPort("echoService").Bind();
  port->PostMessage(std::move(message),
                    [&post_message_response_closure](
                        fuchsia::web::MessagePort_PostMessage_Result result) {
                      ASSERT_TRUE(result.is_response());
                      post_message_response_closure.Run();
                    });

  // Handle the ping response.
  base::test::TestFuture<fuchsia::web::WebMessage> response;
  port->ReceiveMessage(CallbackToFitFunction(response.GetCallback()));
  ASSERT_TRUE(response.Wait());

  std::optional<std::string> response_string =
      base::StringFromMemBuffer(response.Get().data());
  ASSERT_TRUE(response_string.has_value());
  EXPECT_EQ("ack ping", *response_string);

  // Ensure that we've received acks for all messages.
  post_message_responses_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest,
                       ClientDisconnectsBeforeFrameAttached) {
  bool error_signaled = false;
  StartClient(
      true, base::BindOnce([](bool* error_signaled) { *error_signaled = true; },
                           base::Unretained(&error_signaled)));

  // Verify that the error is signalled asynchronously.
  EXPECT_FALSE(error_signaled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(error_signaled);
}

}  // namespace

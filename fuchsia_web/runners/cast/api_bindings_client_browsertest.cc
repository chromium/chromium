// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/api_bindings_client.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/fuchsia/mem_buffer_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chromecast/bindings/bindings_manager_fuchsia.h"
#include "components/cast/message_port/fuchsia/create_web_message.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/runners/cast/named_message_port_connector_fuchsia.h"
#include "fuchsia_web/runners/cast/test/scoped_port_handler.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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
    base::test::TestFuture<void> bindings_future;
    client_ = std::make_unique<ApiBindingsClient>(
        api_service_binding_.NewBinding(), bindings_future.GetCallback());
    ASSERT_FALSE(client_->HasBindings());
    EXPECT_TRUE(bindings_future.Wait());
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
  chromecast::bindings::BindingsManagerFuchsia api_service_;
  fidl::Binding<chromium::cast::ApiBindings> api_service_binding_;
  std::unique_ptr<ApiBindingsClient> client_;
};

// Tests API registration, injection, and message IPC.
// Registers a port that echoes messages received over a MessagePort back to the
// sender.
IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest, EndToEnd) {
  // Define the injected bindings.
  api_service_.AddBinding(
      "echoService",
      "window.echo = cast.__platform__.PortConnector.bind('echoService');");

  ScopedPortHandler echo_port_handler(api_service_, "echoService");

  StartClient(false, base::MakeExpectedNotRunClosure(FROM_HERE));

  // Navigate to a test page that makes use of the injected bindings.
  const GURL test_url = embedded_test_server()->GetURL("/echo.html");
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       test_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(test_url);

  std::string connect_message;
  std::unique_ptr<cast_api_bindings::MessagePort> connect_port;
  connector_->GetConnectMessage(&connect_message, &connect_port);
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result>
      post_frame_message_future;
  frame_->PostMessage(
      "*", CreateWebMessage(connect_message, std::move(connect_port)),
      CallbackToFitFunction(post_frame_message_future.GetCallback()));

  // Wait for the connected port.
  std::unique_ptr<cast_api_bindings::MessagePort> echo_port =
      echo_port_handler.RunUntilPortConnected();
  ASSERT_TRUE(echo_port);

  cast_api_bindings::TestMessagePortReceiver receiver;
  echo_port->SetReceiver(&receiver);
  echo_port->PostMessage("ping");
  receiver.RunUntilMessageCountEqual(1);
  EXPECT_EQ(receiver.buffer()[0].first, "ack ping");

  // Verify SetOrigin updates the backend.
  client_->SetOrigin(url::Origin::Create(test_url));
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return api_service_.origin() == url::Origin::Create(test_url).Serialize();
  }));

  // Verify SetOrigin clears the origin for opaque origins.
  client_->SetOrigin(url::Origin());
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return api_service_.origin().empty(); }));

  // Ensure that we've received ack for the handshake post message.
  EXPECT_TRUE(post_frame_message_future.Get().is_response());
}

IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest,
                       ClientDisconnectsBeforeFrameAttached) {
  bool error_signaled = false;
  StartClient(true, base::BindLambdaForTesting(
                        [&error_signaled]() { error_signaled = true; }));

  // Verify that the error is signalled asynchronously.
  EXPECT_FALSE(error_signaled);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(error_signaled);
}

IN_PROC_BROWSER_TEST_F(ApiBindingsClientTest,
                       SandboxedNavigationKeepsOriginEmpty) {
  net::EmbeddedTestServer sandbox_server;
  sandbox_server.RegisterRequestHandler(base::BindLambdaForTesting(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url == "/sandbox.html") {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_content_type("text/html");
          response->AddCustomHeader("Content-Security-Policy",
                                    "sandbox allow-scripts");
          response->set_content(
              "<!DOCTYPE html><script>"
              "window.addEventListener('DOMContentLoaded', () => {"
              "  if (window.cast && cast.__platform__ &&"
              "      cast.__platform__.PortConnector) {"
              "    cast.__platform__.PortConnector.bind('sandbox_echo');"
              "  }"
              "});"
              "</script>");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(sandbox_server.Start());

  bool sandbox_port_connected = false;
  api_service_.RegisterPortHandler(
      "sandbox_echo", base::BindLambdaForTesting(
                          [&sandbox_port_connected](
                              std::unique_ptr<cast_api_bindings::MessagePort>) {
                            sandbox_port_connected = true;
                          }));
  base::ScopedClosureRunner unregister_sandbox_echo(base::BindOnce(
      [](cast_api_bindings::Manager* manager) {
        manager->UnregisterPortHandler("sandbox_echo");
      },
      base::Unretained(&api_service_)));

  ScopedPortHandler echo_port_handler(api_service_, "echo");

  StartClient(false, base::MakeExpectedNotRunClosure(FROM_HERE));

  // Mirror CastComponent::MaybeConnectPortConnector() so the test can directly
  // wait for both navigation completion and Frame::PostMessage settlement.
  std::string current_url;
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result>
      post_message_future(base::test::TestFutureMode::kQueue);
  base::RepeatingCallback<void(fuchsia::web::Frame_PostMessage_Result)>
      on_post_message = post_message_future.GetRepeatingCallback();
  frame_.navigation_listener().SetBeforeAckHook(base::BindLambdaForTesting(
      [&](const fuchsia::web::NavigationState& change,
          fuchsia::web::NavigationEventListener::
              OnNavigationStateChangedCallback callback) {
        if (change.has_url()) {
          current_url = change.url();
        }
        if (change.has_is_main_document_loaded()) {
          connector_->Reset();
          client_->SetOrigin(url::Origin());
          if (change.is_main_document_loaded()) {
            url::Origin committed_origin =
                url::Origin::Create(GURL(current_url));
            if (!committed_origin.opaque()) {
              std::string connect_message;
              std::unique_ptr<cast_api_bindings::MessagePort> connect_port;
              connector_->GetConnectMessage(&connect_message, &connect_port);
              connector_->RegisterOnConnectedCallback(base::BindOnce(
                  &ApiBindingsClient::SetOrigin,
                  base::Unretained(client_.get()), committed_origin));
              frame_->PostMessage(
                  committed_origin.Serialize(),
                  CreateWebMessage(connect_message, std::move(connect_port)),
                  [on_post_message](
                      fuchsia::web::Frame_PostMessage_Result result) {
                    on_post_message.Run(std::move(result));
                  });
            }
          }
        }
        callback();
      }));
  base::ScopedClosureRunner clear_before_ack_hook(base::BindOnce(
      [](TestNavigationListener* listener) { listener->SetBeforeAckHook({}); },
      base::Unretained(&frame_.navigation_listener())));

  const GURL first_url = embedded_test_server()->GetURL("/connector.html");
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       first_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(first_url);
  ASSERT_TRUE(post_message_future.Take().is_response());

  cast_api_bindings::TestMessagePortReceiver echo_receiver;
  std::unique_ptr<cast_api_bindings::MessagePort> echo_port =
      echo_port_handler.RunUntilPortConnected();
  ASSERT_TRUE(echo_port);
  echo_port->SetReceiver(&echo_receiver);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return api_service_.origin() == url::Origin::Create(first_url).Serialize();
  }));

  // Navigate to a CSP-sandboxed HTTP page whose URL has a tuple origin but
  // whose Blink document has an opaque origin.
  const GURL sandbox_url = sandbox_server.GetURL("/sandbox.html");
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame_.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       sandbox_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(sandbox_url);
  ASSERT_TRUE(post_message_future.Take().is_response());

  // Verify synchronously on the sandboxed document's renderer thread that
  // Blink dropped the target-origin handshake before it could bind
  // PortConnector.controlPort_.
  std::optional<base::Value> control_port_is_null = ExecuteJavaScript(
      frame_.get(), "cast.__platform__.PortConnector.controlPort_ === null");
  ASSERT_TRUE(control_port_is_null.has_value());
  EXPECT_EQ(*control_port_is_null, base::Value(true));
  echo_receiver.RunUntilDisconnected();
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return api_service_.origin().empty(); }));
  EXPECT_FALSE(sandbox_port_connected);
}

}  // namespace

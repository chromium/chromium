// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/named_message_port_connector_fuchsia.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "components/cast/message_port/fuchsia/create_web_message.h"
#include "components/cast/message_port/fuchsia/message_port_fuchsia.h"
#include "components/cast/message_port/test_message_port_receiver.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/test/web_engine_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using CastMessagePort = std::unique_ptr<cast_api_bindings::MessagePort>;

namespace {

class NamedMessagePortConnectorFuchsiaTest : public WebEngineBrowserTest {
 public:
  NamedMessagePortConnectorFuchsiaTest() {
    set_test_server_root(base::FilePath("fuchsia_web/runners/cast/testdata"));
  }

  ~NamedMessagePortConnectorFuchsiaTest() override = default;

  NamedMessagePortConnectorFuchsiaTest(
      const NamedMessagePortConnectorFuchsiaTest&) = delete;
  NamedMessagePortConnectorFuchsiaTest& operator=(
      const NamedMessagePortConnectorFuchsiaTest&) = delete;

 protected:
  // BrowserTestBase implementation.
  void SetUpOnMainThread() override {
    WebEngineBrowserTest::SetUpOnMainThread();
    frame_ = FrameForTest::Create(context(), fuchsia::web::CreateFrameParams());
    frame_.navigation_listener().SetBeforeAckHook(base::BindRepeating(
        &NamedMessagePortConnectorFuchsiaTest::OnBeforeAckHook,
        base::Unretained(this)));
    connector_ =
        std::make_unique<NamedMessagePortConnectorFuchsia>(frame_.get());
  }

  void TearDownOnMainThread() override {
    // Destroy the NamedMessagePortConnector before the Frame.
    connector_ = nullptr;

    // Destroy the Frame before the test terminates
    frame_ = {};

    WebEngineBrowserTest::TearDownOnMainThread();
  }

  // Intercepts the page load event to trigger the injection of |connector_|'s
  // services.
  void OnBeforeAckHook(
      const fuchsia::web::NavigationState& change,
      fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
          callback) {
    if (change.has_url()) {
      committed_origin_ = url::Origin::Create(GURL(change.url()));
    }
    MaybeConnectPortConnector(change);

    // Allow the TestNavigationListener's usual navigation event processing flow
    // to continue.
    callback();
  }

  void MaybeConnectPortConnector(const fuchsia::web::NavigationState& change) {
    if (!change.has_is_main_document_loaded() ||
        !change.is_main_document_loaded()) {
      return;
    }
    if (committed_origin_.opaque()) {
      return;
    }

    std::string connect_message;
    CastMessagePort connect_port;
    connector_->GetConnectMessage(&connect_message, &connect_port);
    frame_->PostMessage(
        committed_origin_.Serialize(),
        CreateWebMessage(connect_message, std::move(connect_port)),
        [](fuchsia::web::Frame_PostMessage_Result result) {
          EXPECT_TRUE(result.is_response());
        });
  }

  FrameForTest frame_;
  std::unique_ptr<NamedMessagePortConnectorFuchsia> connector_;
  url::Origin committed_origin_;
};

IN_PROC_BROWSER_TEST_F(NamedMessagePortConnectorFuchsiaTest, EndToEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/connector.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  base::test::TestFuture<std::string, CastMessagePort> port_future;
  connector_->RegisterPortHandler(base::BindLambdaForTesting(
      [&](std::string_view port_name, CastMessagePort port) {
        port_future.SetValue(std::string(port_name), std::move(port));
        return true;
      }));

  EXPECT_TRUE(LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(test_url);

  // The JS code in connector.html should connect to the port "echo".
  auto [received_port_name, received_port] = port_future.Take();
  EXPECT_EQ(received_port_name, "echo");

  cast_api_bindings::TestMessagePortReceiver test_receiver;
  received_port->SetReceiver(&test_receiver);
  received_port->PostMessage("ping");

  ASSERT_TRUE(test_receiver.RunUntilMessageCountEqual(3));
  EXPECT_EQ(test_receiver.buffer()[0].first, "early 1");
  EXPECT_EQ(test_receiver.buffer()[1].first, "early 2");
  EXPECT_EQ(test_receiver.buffer()[2].first, "ack ping");

  EXPECT_TRUE(received_port->CanPostMessage());

  // Ensure that the MessagePort is dropped when navigating away.
  EXPECT_TRUE(LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), "about:blank"));

  test_receiver.RunUntilDisconnected();
  EXPECT_FALSE(received_port->CanPostMessage());
}

// Tests that the NamedMessagePortConnectorFuchsia can receive more than one
// port over its lifetime.
IN_PROC_BROWSER_TEST_F(NamedMessagePortConnectorFuchsiaTest, MultiplePorts) {
  constexpr size_t kExpectedPortCount = 3;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(
      embedded_test_server()->GetURL("/connector_multiple_ports.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  base::test::TestFuture<std::string, CastMessagePort> ports_future(
      base::test::TestFutureMode::kQueue);
  connector_->RegisterPortHandler(base::BindLambdaForTesting(
      [&](std::string_view port_name, CastMessagePort port) {
        ports_future.SetValue(std::string(port_name), std::move(port));
        return true;
      }));

  EXPECT_TRUE(LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
  frame_.navigation_listener().RunUntilUrlEquals(test_url);

  for (size_t i = 0; i < kExpectedPortCount; ++i) {
    auto [port_name, message_port] = ports_future.Take();
    cast_api_bindings::TestMessagePortReceiver test_receiver;
    message_port->SetReceiver(&test_receiver);
    message_port->PostMessage("ping");
    test_receiver.RunUntilMessageCountEqual(1);
    EXPECT_EQ(test_receiver.buffer()[0].first, "ack ping");
  }
}

}  // namespace

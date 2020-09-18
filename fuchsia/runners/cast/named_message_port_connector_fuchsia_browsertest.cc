// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/message_port.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "fuchsia/runners/cast/named_message_port_connector_fuchsia.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

class NamedMessagePortConnectorFuchsiaTest
    : public cr_fuchsia::WebEngineBrowserTest {
 public:
  NamedMessagePortConnectorFuchsiaTest() {
    set_test_server_root(base::FilePath("fuchsia/runners/cast/testdata"));
    navigation_listener_.SetBeforeAckHook(base::BindRepeating(
        &NamedMessagePortConnectorFuchsiaTest::OnBeforeAckHook,
        base::Unretained(this)));
  }

  ~NamedMessagePortConnectorFuchsiaTest() override = default;

 protected:
  // BrowserTestBase implementation.
  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    frame_ = WebEngineBrowserTest::CreateFrame(&navigation_listener_);
    connector_ =
        std::make_unique<NamedMessagePortConnectorFuchsia>(frame_.get());
  }

  // Intercepts the page load event to trigger the injection of |connector_|'s
  // services.
  void OnBeforeAckHook(
      const fuchsia::web::NavigationState& change,
      fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
          callback) {
    if (change.has_is_main_document_loaded() &&
        change.is_main_document_loaded()) {
      frame_->PostMessage("*",
                          std::move(*cr_fuchsia::FidlWebMessageFromBlink(
                              connector_->GetConnectMessage(),
                              cr_fuchsia::TransferableHostType::kRemote)),
                          [](fuchsia::web::Frame_PostMessage_Result result) {
                            DCHECK(result.is_response());
                          });
    }

    // Allow the TestNavigationListener's usual navigation event processing flow
    // to continue.
    callback();
  }

  std::unique_ptr<base::RunLoop> navigate_run_loop_;
  fuchsia::web::FramePtr frame_;
  std::unique_ptr<NamedMessagePortConnectorFuchsia> connector_;
  cr_fuchsia::TestNavigationListener navigation_listener_;

  DISALLOW_COPY_AND_ASSIGN(NamedMessagePortConnectorFuchsiaTest);
};

IN_PROC_BROWSER_TEST_F(NamedMessagePortConnectorFuchsiaTest, EndToEnd) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url(embedded_test_server()->GetURL("/connector.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame_->GetNavigationController(controller.NewRequest());

  std::string received_port_name;
  fidl::InterfaceHandle<fuchsia::web::MessagePort> received_port;
  base::RunLoop receive_port_run_loop;
  connector_->RegisterPortHandler(base::BindRepeating(
      [](std::string* received_port_name,
         fidl::InterfaceHandle<fuchsia::web::MessagePort>* received_port,
         base::RunLoop* receive_port_run_loop, base::StringPiece port_name,
         blink::WebMessagePort port) -> bool {
        *received_port_name = port_name.as_string();
        *received_port = cr_fuchsia::FidlMessagePortFromBlink(std::move(port));
        receive_port_run_loop->Quit();
        return true;
      },
      base::Unretained(&received_port_name), base::Unretained(&received_port),
      base::Unretained(&receive_port_run_loop)));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url);

  // The JS code in connector.html should connect to the port "echo".
  receive_port_run_loop.Run();
  EXPECT_EQ(received_port_name, "echo");

  fuchsia::web::MessagePortPtr message_port = received_port.Bind();

  fuchsia::web::WebMessage msg;
  msg.set_data(cr_fuchsia::MemBufferFromString("ping", "test"));
  cr_fuchsia::ResultReceiver<fuchsia::web::MessagePort_PostMessage_Result>
      post_result;
  message_port->PostMessage(
      std::move(msg),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

  std::vector<std::string> test_messages = {"early 1", "early 2", "ack ping"};
  for (std::string expected_msg : test_messages) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> message_receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(cr_fuchsia::CallbackToFitFunction(
        message_receiver.GetReceiveCallback()));
    run_loop.Run();

    std::string data;
    ASSERT_TRUE(message_receiver->has_data());
    ASSERT_TRUE(
        cr_fuchsia::StringFromMemBuffer(message_receiver->data(), &data));
    EXPECT_EQ(data, expected_msg);
  }

  // Ensure that the MessagePort is dropped when navigating away.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler([&run_loop](zx_status_t status) {
      EXPECT_EQ(ZX_ERR_PEER_CLOSED, status);
      run_loop.Quit();
    });
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), "about:blank"));
    run_loop.Run();
  }
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

  std::vector<fidl::InterfaceHandle<fuchsia::web::MessagePort>> received_ports;
  base::RunLoop receive_ports_run_loop;
  connector_->RegisterPortHandler(base::BindRepeating(
      [](std::vector<fidl::InterfaceHandle<fuchsia::web::MessagePort>>*
             received_ports,
         base::RunLoop* receive_ports_run_loop, base::StringPiece,
         blink::WebMessagePort port) -> bool {
        received_ports->push_back(
            cr_fuchsia::FidlMessagePortFromBlink(std::move(port)));

        if (received_ports->size() == kExpectedPortCount)
          receive_ports_run_loop->Quit();

        return true;
      },
      base::Unretained(&received_ports),
      base::Unretained(&receive_ports_run_loop)));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
  navigation_listener_.RunUntilUrlEquals(test_url);
  receive_ports_run_loop.Run();

  for (fidl::InterfaceHandle<fuchsia::web::MessagePort>& message_port :
       received_ports) {
    fuchsia::web::MessagePortPtr port = message_port.Bind();
    fuchsia::web::WebMessage msg;
    msg.set_data(cr_fuchsia::MemBufferFromString("ping", "test"));
    cr_fuchsia::ResultReceiver<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    port->PostMessage(std::move(msg), cr_fuchsia::CallbackToFitFunction(
                                          post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> message_receiver(
        run_loop.QuitClosure());
    port->ReceiveMessage(cr_fuchsia::CallbackToFitFunction(
        message_receiver.GetReceiveCallback()));
    run_loop.Run();

    std::string data;
    ASSERT_TRUE(message_receiver->has_data());
    ASSERT_TRUE(
        cr_fuchsia::StringFromMemBuffer(message_receiver->data(), &data));
    EXPECT_EQ(data, "ack ping");
  }
}

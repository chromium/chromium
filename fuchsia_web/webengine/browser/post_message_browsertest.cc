// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/fuchsia/mem_buffer_util.h"
#include "base/test/test_future.h"
#include "content/public/test/browser_test.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage1Title[] = "title 1";

// Defines a suite of tests that exercise Frame-level post message
// functionality.
class PostMessageTest : public FrameImplTestBase {
 public:
  PostMessageTest() = default;
  ~PostMessageTest() override = default;

  PostMessageTest(const PostMessageTest&) = delete;
  PostMessageTest& operator=(const PostMessageTest&) = delete;
};

IN_PROC_BROWSER_TEST_F(PostMessageTest, SendData) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(
      embedded_test_server()->GetURL("/window_post_message.html"));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "postmessage");

  fuchsia::web::WebMessage message;
  message.set_data(base::MemBufferFromString(kPage1Path, "test"));
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
  frame->PostMessage(post_message_url.DeprecatedGetOriginAsURL().spec(),
                     std::move(message),
                     CallbackToFitFunction(post_result.GetCallback()));
  ASSERT_TRUE(post_result.Wait());

  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      embedded_test_server()->GetURL(kPage1Path), kPage1Title);

  EXPECT_TRUE(post_result.Get().is_response());
}

// Send a MessagePort to the content, then perform bidirectional messaging
// through the port.
IN_PROC_BROWSER_TEST_F(PostMessageTest, PassMessagePort) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr message_port;
  {
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        post_message_url.DeprecatedGetOriginAsURL().spec(),
        CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", *base::StringFromMemBuffer(receiver.Get().data()));
  }

  {
    fuchsia::web::WebMessage msg;
    msg.set_data(base::MemBufferFromString("ping", "test"));
    base::test::TestFuture<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    message_port->PostMessage(std::move(msg),
                              CallbackToFitFunction(post_result.GetCallback()));
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("ack ping", *base::StringFromMemBuffer(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }
}

// Send a MessagePort to the content, then perform bidirectional messaging
// over its channel.
IN_PROC_BROWSER_TEST_F(PostMessageTest, MessagePortDisconnected) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr message_port;
  {
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        post_message_url.DeprecatedGetOriginAsURL().spec(),
        CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.IsReady());
    EXPECT_EQ("got_port", *base::StringFromMemBuffer(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Navigating off-page should tear down the Mojo channel, thereby causing the
  // MessagePortImpl to self-destruct and tear down its FIDL channel.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler(
        [&run_loop](zx_status_t) { run_loop.Quit(); });
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         url::kAboutBlankURL));
    run_loop.Run();
  }
}

// Send a MessagePort to the content, and through that channel, receive a
// different MessagePort that was created by the content. Verify the second
// channel's liveness by sending a ping to it.
IN_PROC_BROWSER_TEST_F(PostMessageTest, UseContentProvidedPort) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr incoming_message_port;
  {
    fuchsia::web::MessagePortPtr message_port;
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        "*",
        CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", *base::StringFromMemBuffer(receiver.Get().data()));
    ASSERT_TRUE(receiver.Get().has_incoming_transfer());
    ASSERT_EQ(receiver.Get().incoming_transfer().size(), 1u);
    incoming_message_port = receiver.Take()
                                .mutable_incoming_transfer()
                                ->at(0)
                                .message_port()
                                .Bind();
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Get the content to send three 'ack ping' messages, which will accumulate in
  // the MessagePortImpl buffer.
  for (int i = 0; i < 3; ++i) {
    base::test::TestFuture<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    fuchsia::web::WebMessage msg;
    msg.set_data(base::MemBufferFromString("ping", "test"));
    incoming_message_port->PostMessage(
        std::move(msg), CallbackToFitFunction(post_result.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Receive another acknowledgement from content on a side channel to ensure
  // that all the "ack pings" are ready to be consumed.
  {
    fuchsia::web::MessagePortPtr ack_message_port;

    // Quit the runloop only after we've received a WebMessage AND a PostMessage
    // result.
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage("*",
                       CreateWebMessageWithMessagePortRequest(
                           ack_message_port.NewRequest(),
                           base::MemBufferFromString("hi", "test")),
                       CallbackToFitFunction(post_result.GetCallback()));
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    ack_message_port->ReceiveMessage(
        CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", *base::StringFromMemBuffer(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Pull the three 'ack ping's from the buffer.
  for (int i = 0; i < 3; ++i) {
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    incoming_message_port->ReceiveMessage(
        CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("ack ping", *base::StringFromMemBuffer(receiver.Get().data()));
  }
}

IN_PROC_BROWSER_TEST_F(PostMessageTest, BadOriginDropped) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  // PostMessage() to invalid origins should be ignored. We pass in a
  // MessagePort but nothing should happen to it.
  fuchsia::web::MessagePortPtr bad_origin_incoming_message_port;
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result>
      unused_post_result;
  frame->PostMessage("https://example.com",
                     CreateWebMessageWithMessagePortRequest(
                         bad_origin_incoming_message_port.NewRequest(),
                         base::MemBufferFromString("bad origin, bad!", "test")),
                     CallbackToFitFunction(unused_post_result.GetCallback()));
  base::test::TestFuture<fuchsia::web::WebMessage> unused_message_read;
  bad_origin_incoming_message_port->ReceiveMessage(
      CallbackToFitFunction(unused_message_read.GetCallback()));

  // PostMessage() with a valid origin should succeed.
  // Verify it by looking for an ack message on the MessagePort we passed in.
  // Since message events are handled in order, observing the result of this
  // operation will verify whether the previous PostMessage() was received but
  // discarded.
  fuchsia::web::MessagePortPtr incoming_message_port;
  fuchsia::web::MessagePortPtr message_port;
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
  frame->PostMessage("*",
                     CreateWebMessageWithMessagePortRequest(
                         message_port.NewRequest(),
                         base::MemBufferFromString("good origin", "test")),
                     CallbackToFitFunction(post_result.GetCallback()));
  base::test::TestFuture<fuchsia::web::WebMessage> receiver;
  message_port->ReceiveMessage(CallbackToFitFunction(receiver.GetCallback()));
  ASSERT_TRUE(receiver.Wait());
  ASSERT_TRUE(receiver.Get().has_data());
  EXPECT_EQ("got_port", *base::StringFromMemBuffer(receiver.Get().data()));
  ASSERT_TRUE(receiver.Get().has_incoming_transfer());
  ASSERT_EQ(receiver.Get().incoming_transfer().size(), 1u);
  incoming_message_port =
      receiver.Take().mutable_incoming_transfer()->at(0).message_port().Bind();
  EXPECT_TRUE(post_result.Get().is_response());

  // Verify that the first PostMessage() call wasn't handled.
  EXPECT_FALSE(unused_message_read.IsReady());
}

}  // namespace

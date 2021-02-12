// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/threading/platform_thread.h"
#include "components/cast/message_port/message_port_fuchsia.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/cast_streaming/test/cast_streaming_test_sender.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "media/base/media_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kCastStreamingReceiverPath[] = "/cast_streaming_receiver.html";

media::AudioDecoderConfig GetDefaultAudioConfig() {
  return media::AudioDecoderConfig(
      media::AudioCodec::kCodecOpus, media::SampleFormat::kSampleFormatF32,
      media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
      48000 /* samples_per_second */, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);
}

media::VideoDecoderConfig GetDefaultVideoConfig() {
  const gfx::Size kVideoSize = {1920, 1080};
  const gfx::Rect kVideoRect(kVideoSize);

  return media::VideoDecoderConfig(
      media::VideoCodec::kCodecVP8, media::VideoCodecProfile::VP8PROFILE_MIN,
      media::VideoDecoderConfig::AlphaMode::kIsOpaque, media::VideoColorSpace(),
      media::VideoTransformation(), kVideoSize, kVideoRect, kVideoSize,
      media::EmptyExtraData(), media::EncryptionScheme::kUnencrypted);
}

}  // namespace

// Base test fixture for Cast Streaming tests.
class CastStreamingBaseTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  CastStreamingBaseTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~CastStreamingBaseTest() override = default;

  CastStreamingBaseTest(const CastStreamingBaseTest&) = delete;
  CastStreamingBaseTest& operator=(const CastStreamingBaseTest&) = delete;

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;
};

// Test fixture for Cast Streaming tests with the Cast Streaming Receiver flag
// disabled.
class CastStreamingDisabledTest : public CastStreamingBaseTest {
 public:
  CastStreamingDisabledTest() = default;
  ~CastStreamingDisabledTest() override = default;
  CastStreamingDisabledTest(const CastStreamingDisabledTest&) = delete;
  CastStreamingDisabledTest& operator=(const CastStreamingDisabledTest&) =
      delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::BrowserTestBase::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(switches::kEnableCastStreamingReceiver);
  }
};

// Test fixture for Cast Streaming tests with the Cast Streaming Receiver flag
// enabled.
class CastStreamingTest : public CastStreamingBaseTest {
 public:
  CastStreamingTest() = default;
  ~CastStreamingTest() override = default;
  CastStreamingTest(const CastStreamingTest&) = delete;
  CastStreamingTest& operator=(const CastStreamingTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::BrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableCastStreamingReceiver);
  }
};

// Check that attempting to load the cast streaming media source URL when the
// command line switch is not set fails as expected.
IN_PROC_BROWSER_TEST_F(CastStreamingDisabledTest, LoadFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page_url(embedded_test_server()->GetURL(kCastStreamingReceiverPath));

  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilTitleEquals("error");
}

// Check that the Cast Streaming MessagePort gets properly set on the Frame.
IN_PROC_BROWSER_TEST_F(CastStreamingTest, FrameMessagePort) {
  fuchsia::web::FramePtr frame = CreateFrame();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  ASSERT_TRUE(frame_impl);
  EXPECT_FALSE(frame_impl->cast_streaming_session_client_for_test());

  fuchsia::web::MessagePortPtr cast_streaming_message_port;

  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
      post_result(run_loop.QuitClosure());
  frame->PostMessage(
      "cast-streaming:receiver",
      cr_fuchsia::CreateWebMessageWithMessagePortRequest(
          cast_streaming_message_port.NewRequest(),
          cr_fuchsia::MemBufferFromString("hi", "test")),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
  run_loop.Run();
  ASSERT_TRUE(post_result->is_response());

  EXPECT_TRUE(frame_impl->cast_streaming_session_client_for_test());
}

// Check that attempting to load the cast streaming media source URL when the
// command line switch is set properly succeeds.
IN_PROC_BROWSER_TEST_F(CastStreamingTest, LoadSuccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kPageUrl(
      embedded_test_server()->GetURL(kCastStreamingReceiverPath));
  fuchsia::mem::Buffer ignored_message_string =
      cr_fuchsia::MemBufferFromString("hi", "test");

  std::unique_ptr<cast_api_bindings::MessagePort> sender_message_port;
  std::unique_ptr<cast_api_bindings::MessagePort> receiver_message_port;
  cast_api_bindings::MessagePort::CreatePair(&sender_message_port,
                                             &receiver_message_port);

  fidl::InterfaceRequest<::fuchsia::web::MessagePort> message_port_request =
      cast_api_bindings::MessagePortFuchsia::FromMessagePort(
          receiver_message_port.get())
          ->TakeServiceRequest();

  // Start the Sender
  cast_streaming::CastStreamingTestSender sender;
  EXPECT_TRUE(sender.Start(std::move(sender_message_port),
                           net::IPAddress::IPv6Localhost(),
                           GetDefaultAudioConfig(), GetDefaultVideoConfig()));

  // Create a Frame and set the Receiver MessagePort on it.
  fuchsia::web::FramePtr frame = CreateFrame();
  cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
      post_result(base::DoNothing::Repeatedly());
  frame->PostMessage(
      "cast-streaming:receiver",
      cr_fuchsia::CreateWebMessageWithMessagePortRequest(
          std::move(message_port_request), std::move(ignored_message_string)),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kPageUrl.spec()));

  sender.RunUntilStarted();
  navigation_listener_.RunUntilTitleEquals("canplay");
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojom/remoting_mojom_traits.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "remoting/base/ipc_fifo_buffer.h"
#include "remoting/base/source_location.h"
#include "remoting/host/base/desktop_environment_options.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "remoting/host/mojom/remoting_host.mojom.h"
#include "remoting/host/mojom/webrtc_types.mojom.h"
#include "remoting/proto/audio.pb.h"
#include "remoting/proto/control.pb.h"
#include "remoting/proto/event.pb.h"
#include "remoting/proto/file_transfer.pb.h"
#include "remoting/protocol/transport.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

struct DesktopCaptureOptionsParams {
  bool use_update_notifications;
  bool detect_updated_region;
#if BUILDFLAG(IS_WIN)
  bool allow_directx_capturer;
#endif
};

class DesktopCaptureOptionsTest
    : public testing::TestWithParam<DesktopCaptureOptionsParams> {};

TEST_P(DesktopCaptureOptionsTest, RoundTrip) {
  const auto& params = GetParam();
  webrtc::DesktopCaptureOptions input;
  input.set_use_update_notifications(params.use_update_notifications);
  input.set_detect_updated_region(params.detect_updated_region);
#if BUILDFLAG(IS_WIN)
  input.set_allow_directx_capturer(params.allow_directx_capturer);
#endif

  webrtc::DesktopCaptureOptions output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DesktopCaptureOptions>(
      input, output));

  EXPECT_EQ(input.use_update_notifications(),
            output.use_update_notifications());
  EXPECT_EQ(input.detect_updated_region(), output.detect_updated_region());
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(input.allow_directx_capturer(), output.allow_directx_capturer());
#endif
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         DesktopCaptureOptionsTest,
                         testing::Values(
#if BUILDFLAG(IS_WIN)
                             DesktopCaptureOptionsParams{false, false, false},
                             DesktopCaptureOptionsParams{false, false, true},
                             DesktopCaptureOptionsParams{false, true, false},
                             DesktopCaptureOptionsParams{false, true, true},
                             DesktopCaptureOptionsParams{true, false, false},
                             DesktopCaptureOptionsParams{true, false, true},
                             DesktopCaptureOptionsParams{true, true, false},
                             DesktopCaptureOptionsParams{true, true, true}
#else
                             DesktopCaptureOptionsParams{false, false},
                             DesktopCaptureOptionsParams{false, true},
                             DesktopCaptureOptionsParams{true, false},
                             DesktopCaptureOptionsParams{true, true}
#endif
                             ));

struct DesktopEnvironmentOptionsParams {
  bool enable_curtaining;
  bool enable_user_interface;
  bool enable_notifications;
  bool terminate_upon_input;
  bool enable_remote_webauthn;
};

class DesktopEnvironmentOptionsTest
    : public testing::TestWithParam<DesktopEnvironmentOptionsParams> {};

TEST_P(DesktopEnvironmentOptionsTest, RoundTrip) {
  const auto& params = GetParam();
  DesktopEnvironmentOptions input = DesktopEnvironmentOptions::CreateDefault();
  input.set_enable_curtaining(params.enable_curtaining);
  input.set_enable_user_interface(params.enable_user_interface);
  input.set_enable_notifications(params.enable_notifications);
  input.set_terminate_upon_input(params.terminate_upon_input);
  input.set_enable_remote_webauthn(params.enable_remote_webauthn);

  DesktopEnvironmentOptions output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DesktopEnvironmentOptions>(
          input, output));

  EXPECT_EQ(input.enable_curtaining(), output.enable_curtaining());
  EXPECT_EQ(input.enable_user_interface(), output.enable_user_interface());
  EXPECT_EQ(input.enable_notifications(), output.enable_notifications());
  EXPECT_EQ(input.terminate_upon_input(), output.terminate_upon_input());
  EXPECT_EQ(input.enable_remote_webauthn(), output.enable_remote_webauthn());
}

INSTANTIATE_TEST_SUITE_P(
    RemotingMojomTraitsTest,
    DesktopEnvironmentOptionsTest,
    testing::Values(
        DesktopEnvironmentOptionsParams{false, false, false, false, false},
        DesktopEnvironmentOptionsParams{true, true, true, true, true}));

class DesktopCaptureResultTest
    : public testing::TestWithParam<webrtc::DesktopCapturer::Result> {};

TEST_P(DesktopCaptureResultTest, RoundTrip) {
  webrtc::DesktopCapturer::Result input = GetParam();
  webrtc::DesktopCapturer::Result output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DesktopCaptureResult>(
      input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(
    RemotingMojomTraitsTest,
    DesktopCaptureResultTest,
    testing::Values(webrtc::DesktopCapturer::Result::SUCCESS,
                    webrtc::DesktopCapturer::Result::ERROR_TEMPORARY,
                    webrtc::DesktopCapturer::Result::ERROR_PERMANENT));

class MouseButtonTest
    : public testing::TestWithParam<protocol::MouseEvent::MouseButton> {};

TEST_P(MouseButtonTest, RoundTrip) {
  protocol::MouseEvent::MouseButton input = GetParam();
  protocol::MouseEvent::MouseButton output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::MouseButton>(input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         MouseButtonTest,
                         testing::Values(protocol::MouseEvent::BUTTON_UNDEFINED,
                                         protocol::MouseEvent::BUTTON_LEFT,
                                         protocol::MouseEvent::BUTTON_MIDDLE,
                                         protocol::MouseEvent::BUTTON_RIGHT,
                                         protocol::MouseEvent::BUTTON_BACK,
                                         protocol::MouseEvent::BUTTON_FORWARD));

class AudioPacketBytesPerSampleTest
    : public testing::TestWithParam<AudioPacket::BytesPerSample> {};

TEST_P(AudioPacketBytesPerSampleTest, RoundTrip) {
  AudioPacket::BytesPerSample input = GetParam();
  AudioPacket::BytesPerSample output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::AudioPacket_BytesPerSample>(
          input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         AudioPacketBytesPerSampleTest,
                         testing::Values(AudioPacket::BYTES_PER_SAMPLE_INVALID,
                                         AudioPacket::BYTES_PER_SAMPLE_2));

class AudioPacketChannelsTest
    : public testing::TestWithParam<AudioPacket::Channels> {};

TEST_P(AudioPacketChannelsTest, RoundTrip) {
  AudioPacket::Channels input = GetParam();
  AudioPacket::Channels output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioPacket_Channels>(
      input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         AudioPacketChannelsTest,
                         testing::Values(AudioPacket::CHANNELS_INVALID,
                                         AudioPacket::CHANNELS_MONO,
                                         AudioPacket::CHANNELS_STEREO,
                                         AudioPacket::CHANNELS_SURROUND,
                                         AudioPacket::CHANNELS_4_0,
                                         AudioPacket::CHANNELS_4_1,
                                         AudioPacket::CHANNELS_5_1,
                                         AudioPacket::CHANNELS_6_1,
                                         AudioPacket::CHANNELS_7_1));

class AudioPacketEncodingTest
    : public testing::TestWithParam<AudioPacket::Encoding> {};

TEST_P(AudioPacketEncodingTest, RoundTrip) {
  AudioPacket::Encoding input = GetParam();
  AudioPacket::Encoding output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AudioPacket_Encoding>(
      input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         AudioPacketEncodingTest,
                         testing::Values(AudioPacket::ENCODING_INVALID,
                                         AudioPacket::ENCODING_RAW,
                                         AudioPacket::ENCODING_OPUS));

class AudioPacketSamplingRateTest
    : public testing::TestWithParam<AudioPacket::SamplingRate> {};

TEST_P(AudioPacketSamplingRateTest, RoundTrip) {
  AudioPacket::SamplingRate input = GetParam();
  AudioPacket::SamplingRate output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::AudioPacket_SamplingRate>(
          input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         AudioPacketSamplingRateTest,
                         testing::Values(AudioPacket::SAMPLING_RATE_INVALID,
                                         AudioPacket::SAMPLING_RATE_44100,
                                         AudioPacket::SAMPLING_RATE_48000));

class ProtocolErrorCodeTest
    : public testing::TestWithParam<protocol::ErrorCode> {};

TEST_P(ProtocolErrorCodeTest, RoundTrip) {
  protocol::ErrorCode input = GetParam();
  protocol::ErrorCode output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ProtocolErrorCode>(
      input, output));
  EXPECT_EQ(input, output);
}

INSTANTIATE_TEST_SUITE_P(
    RemotingMojomTraitsTest,
    ProtocolErrorCodeTest,
    testing::Values(protocol::ErrorCode::OK,
                    protocol::ErrorCode::PEER_IS_OFFLINE,
                    protocol::ErrorCode::SESSION_REJECTED,
                    protocol::ErrorCode::INCOMPATIBLE_PROTOCOL,
                    protocol::ErrorCode::AUTHENTICATION_FAILED,
                    protocol::ErrorCode::INVALID_ACCOUNT,
                    protocol::ErrorCode::CHANNEL_CONNECTION_ERROR,
                    protocol::ErrorCode::SIGNALING_ERROR,
                    protocol::ErrorCode::SIGNALING_TIMEOUT,
                    protocol::ErrorCode::HOST_OVERLOAD,
                    protocol::ErrorCode::MAX_SESSION_LENGTH,
                    protocol::ErrorCode::HOST_CONFIGURATION_ERROR,
                    protocol::ErrorCode::UNKNOWN_ERROR,
                    protocol::ErrorCode::ELEVATION_ERROR,
                    protocol::ErrorCode::HOST_CERTIFICATE_ERROR,
                    protocol::ErrorCode::HOST_REGISTRATION_ERROR,
                    protocol::ErrorCode::EXISTING_ADMIN_SESSION,
                    protocol::ErrorCode::AUTHZ_POLICY_CHECK_FAILED,
                    protocol::ErrorCode::DISALLOWED_BY_POLICY,
                    protocol::ErrorCode::LOCATION_AUTHZ_POLICY_CHECK_FAILED,
                    protocol::ErrorCode::UNAUTHORIZED_ACCOUNT,
                    protocol::ErrorCode::REAUTHZ_POLICY_CHECK_FAILED,
                    protocol::ErrorCode::NO_COMMON_AUTH_METHOD,
                    protocol::ErrorCode::LOGIN_SCREEN_NOT_SUPPORTED,
                    protocol::ErrorCode::SESSION_POLICIES_CHANGED,
                    protocol::ErrorCode::UNEXPECTED_AUTHENTICATOR_ERROR,
                    protocol::ErrorCode::INVALID_STATE,
                    protocol::ErrorCode::INVALID_ARGUMENT,
                    protocol::ErrorCode::NETWORK_FAILURE,
                    protocol::ErrorCode::OPERATION_TIMEOUT));

}  // namespace

TEST(RemotingMojomTraitsTest, DesktopRect) {
  webrtc::DesktopRect input = webrtc::DesktopRect::MakeLTRB(10, 20, 30, 40);
  webrtc::DesktopRect output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DesktopRect>(input, output));
  EXPECT_TRUE(input.equals(output));
}

TEST(RemotingMojomTraitsTest, DesktopSize) {
  webrtc::DesktopSize input(100, 200);
  webrtc::DesktopSize output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DesktopSize>(input, output));
  EXPECT_TRUE(input.equals(output));
}

TEST(RemotingMojomTraitsTest, DesktopVector) {
  webrtc::DesktopVector input(5, 10);
  webrtc::DesktopVector output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DesktopVector>(input, output));
  EXPECT_TRUE(input.equals(output));
}

TEST(RemotingMojomTraitsTest, MouseCursor) {
  webrtc::DesktopSize size(10, 10);
  auto image =
      std::make_unique<webrtc::BasicDesktopFrame>(size, webrtc::FOURCC_ARGB);
  // Fill image with some data.
  // SAFETY: BasicDesktopFrame::data() returns a buffer of size width * height *
  // 4 (kBytesPerPixel).
  base::span<uint8_t> image_data = UNSAFE_BUFFERS(
      base::span(image->data(),
                 static_cast<size_t>(size.width() * size.height() *
                                     webrtc::DesktopFrame::kBytesPerPixel)));
  std::ranges::fill(image_data, 0xFF);

  webrtc::MouseCursor input(std::move(image), webrtc::DesktopVector(5, 5));

  std::vector<uint8_t> data = mojom::MouseCursor::Serialize(&input);

  webrtc::MouseCursor output;
  ASSERT_TRUE(mojom::MouseCursor::Deserialize(data, &output));

  ASSERT_TRUE(output.image());
  ASSERT_TRUE(input.image()->size().equals(output.image()->size()));
  EXPECT_TRUE(input.hotspot().equals(output.hotspot()));

  // SAFETY: The images are expected to have the same size as the input.
  base::span<const uint8_t> input_data = UNSAFE_BUFFERS(
      base::span(input.image()->data(),
                 static_cast<size_t>(size.width() * size.height() *
                                     webrtc::DesktopFrame::kBytesPerPixel)));
  base::span<const uint8_t> output_data = UNSAFE_BUFFERS(
      base::span(output.image()->data(),
                 static_cast<size_t>(size.width() * size.height() *
                                     webrtc::DesktopFrame::kBytesPerPixel)));
  EXPECT_EQ(input_data, output_data);
}

TEST(RemotingMojomTraitsTest, AudioPacket) {
  auto input = std::make_unique<AudioPacket>();
  input->set_timestamp(12345);
  input->add_data("test_data");
  input->set_encoding(AudioPacket::ENCODING_OPUS);
  input->set_sampling_rate(AudioPacket::SAMPLING_RATE_48000);
  input->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  input->set_channels(AudioPacket::CHANNELS_STEREO);

  std::unique_ptr<AudioPacket> output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::AudioPacket>(input, output));

  EXPECT_EQ(input->timestamp(), output->timestamp());
  ASSERT_EQ(input->data_size(), output->data_size());
  EXPECT_EQ(input->data(0), output->data(0));
  EXPECT_EQ(input->encoding(), output->encoding());
  EXPECT_EQ(input->sampling_rate(), output->sampling_rate());
  EXPECT_EQ(input->bytes_per_sample(), output->bytes_per_sample());
  EXPECT_EQ(input->channels(), output->channels());
}

TEST(RemotingMojomTraitsTest, ClipboardEvent) {
  protocol::ClipboardEvent input;
  input.set_mime_type("text/plain");
  input.set_data("clipboard_data");

  protocol::ClipboardEvent output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ClipboardEvent>(
      input, output));

  EXPECT_EQ(input.mime_type(), output.mime_type());
  EXPECT_EQ(input.data(), output.data());
}

TEST(RemotingMojomTraitsTest, ReadChunkResult) {
  {
    std::vector<uint8_t> data = {1, 2, 3, 4};
    protocol::FileTransferResult<std::vector<uint8_t>> input;
    input.EmplaceSuccess(data);

    protocol::FileTransferResult<std::vector<uint8_t>> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ReadChunkResult>(
        input, output));
    EXPECT_TRUE(output.is_success());
    EXPECT_EQ(data, output.success());
  }
  {
    protocol::FileTransfer_Error error;
    error.set_type(protocol::FileTransfer_Error::IO_ERROR);
    protocol::FileTransferResult<std::vector<uint8_t>> input;
    input.EmplaceError(error);

    protocol::FileTransferResult<std::vector<uint8_t>> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ReadChunkResult>(
        input, output));
    EXPECT_TRUE(output.is_error());
    EXPECT_EQ(error.type(), output.error().type());
  }
}

TEST(RemotingMojomTraitsTest, FileTransferError) {
  protocol::FileTransfer_Error input;
  input.set_type(protocol::FileTransfer_Error::IO_ERROR);
  input.set_api_error_code(123);
  input.set_function("test_function");
  input.set_source_file("test_file.cc");
  input.set_line_number(42);

  protocol::FileTransfer_Error output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FileTransferError>(
      input, output));

  EXPECT_EQ(input.type(), output.type());
  EXPECT_EQ(input.api_error_code(), output.api_error_code());
  EXPECT_EQ(input.function(), output.function());
  EXPECT_EQ(input.source_file(), output.source_file());
  EXPECT_EQ(input.line_number(), output.line_number());
}

TEST(RemotingMojomTraitsTest, KeyboardLayout) {
  protocol::KeyboardLayout input;
  auto& key_behavior = (*input.mutable_keys())[0x070004];  // USB Key A
  auto& key_action = (*key_behavior.mutable_actions())[0];
  key_action.set_character("a");

  protocol::KeyboardLayout output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::KeyboardLayout>(
      input, output));

  EXPECT_EQ(input.keys().size(), output.keys().size());
  EXPECT_EQ(input.keys().at(0x070004).actions().at(0).character(),
            output.keys().at(0x070004).actions().at(0).character());
}

TEST(RemotingMojomTraitsTest, KeyAction) {
  {
    protocol::KeyboardLayout::KeyAction input;
    input.set_function(protocol::LayoutKeyFunction::CONTROL);

    protocol::KeyboardLayout::KeyAction output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::KeyAction>(input, output));
    EXPECT_EQ(input.action_case(), output.action_case());
    EXPECT_EQ(input.function(), output.function());
  }
  {
    protocol::KeyboardLayout::KeyAction input;
    input.set_character("a");

    protocol::KeyboardLayout::KeyAction output;
    ASSERT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::KeyAction>(input, output));
    EXPECT_EQ(input.action_case(), output.action_case());
    EXPECT_EQ(input.character(), output.character());
  }
}

TEST(RemotingMojomTraitsTest, KeyBehavior) {
  protocol::KeyboardLayout::KeyBehavior input;
  auto& key_action = (*input.mutable_actions())[0];
  key_action.set_character("a");

  protocol::KeyboardLayout::KeyBehavior output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::KeyBehavior>(input, output));

  EXPECT_EQ(input.actions().size(), output.actions().size());
  EXPECT_EQ(input.actions().at(0).character(),
            output.actions().at(0).character());
}

#if BUILDFLAG(IS_WIN)
TEST(RemotingMojomTraitsTest, FileChooserResult) {
  {
    base::FilePath path(FILE_PATH_LITERAL("C:\\test\\file.txt"));
    protocol::FileTransferResult<base::FilePath> input;
    input.EmplaceSuccess(path);

    protocol::FileTransferResult<base::FilePath> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FileChooserResult>(
        input, output));
    EXPECT_TRUE(output.is_success());
    EXPECT_EQ(path, output.success());
  }
  {
    protocol::FileTransfer_Error error;
    error.set_type(protocol::FileTransfer_Error::IO_ERROR);
    protocol::FileTransferResult<base::FilePath> input;
    input.EmplaceError(error);

    protocol::FileTransferResult<base::FilePath> output;
    ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FileChooserResult>(
        input, output));
    EXPECT_TRUE(output.is_error());
    EXPECT_EQ(error.type(), output.error().type());
  }
}
#endif

TEST(RemotingMojomTraitsTest, ScreenResolution) {
  ScreenResolution input(webrtc::DesktopSize(1920, 1080),
                         webrtc::DesktopVector(96, 96));
  ScreenResolution output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ScreenResolution>(
      input, output));

  EXPECT_TRUE(input.dimensions().equals(output.dimensions()));
  EXPECT_TRUE(input.dpi().equals(output.dpi()));
}

TEST(RemotingMojomTraitsTest, TextEvent) {
  protocol::TextEvent input;
  input.set_text("test_text");

  protocol::TextEvent output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::TextEvent>(input, output));

  EXPECT_EQ(input.text(), output.text());
}

TEST(RemotingMojomTraitsTest, TouchEvent) {
  protocol::TouchEvent input;
  input.set_event_type(protocol::TouchEvent::TOUCH_POINT_START);
  auto* point = input.add_touch_points();
  point->set_id(1);
  point->set_x(100.0f);
  point->set_y(200.0f);
  point->set_radius_x(5.0f);
  point->set_radius_y(10.0f);
  point->set_angle(45.0f);
  point->set_pressure(0.5f);

  protocol::TouchEvent output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::TouchEvent>(input, output));

  EXPECT_EQ(input.event_type(), output.event_type());
  ASSERT_EQ(input.touch_points_size(), output.touch_points_size());
  EXPECT_EQ(input.touch_points(0).id(), output.touch_points(0).id());
  EXPECT_EQ(input.touch_points(0).x(), output.touch_points(0).x());
  EXPECT_EQ(input.touch_points(0).y(), output.touch_points(0).y());
}

TEST(RemotingMojomTraitsTest, TransportRoute) {
  protocol::TransportRoute input;
  input.type = protocol::TransportRoute::DIRECT;
  input.remote_address = net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 1234);
  input.local_address = net::IPEndPoint(net::IPAddress(5, 6, 7, 8), 5678);

  protocol::TransportRoute output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransportRoute>(
      input, output));

  EXPECT_EQ(input.type, output.type);
  EXPECT_EQ(input.remote_address, output.remote_address);
  EXPECT_EQ(input.local_address, output.local_address);
}

TEST(RemotingMojomTraitsTest, VideoTrackLayout) {
  protocol::VideoTrackLayout input;
  input.set_screen_id(123);
  input.set_media_stream_id("test_stream");
  input.set_position_x(10);
  input.set_position_y(20);
  input.set_width(100);
  input.set_height(200);
  input.set_x_dpi(96);
  input.set_y_dpi(96);
  input.set_display_name("test_display");

  protocol::VideoTrackLayout output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::VideoTrackLayout>(
      input, output));

  EXPECT_EQ(input.screen_id(), output.screen_id());
  EXPECT_EQ(input.media_stream_id(), output.media_stream_id());
  EXPECT_EQ(input.position_x(), output.position_x());
  EXPECT_EQ(input.position_y(), output.position_y());
  EXPECT_EQ(input.width(), output.width());
  EXPECT_EQ(input.height(), output.height());
  EXPECT_EQ(input.x_dpi(), output.x_dpi());
  EXPECT_EQ(input.y_dpi(), output.y_dpi());
  EXPECT_EQ(input.display_name(), output.display_name());
}

TEST(RemotingMojomTraitsTest, SourceLocation) {
  SourceLocation input = SourceLocation::CreateWithBackingStoreForTesting(
      "test_func", "test_file.cc", 123);
  SourceLocation output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SourceLocation>(
      input, output));

  EXPECT_STREQ(input.function_name(), output.function_name());
  EXPECT_STREQ(input.file_name(), output.file_name());
  EXPECT_EQ(input.line_number(), output.line_number());
}

TEST(RemotingMojomTraitsTest, FractionalCoordinate) {
  protocol::FractionalCoordinate input;
  input.set_x(0.5f);
  input.set_y(0.6f);
  input.set_screen_id(1);

  protocol::FractionalCoordinate output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::FractionalCoordinate>(
      input, output));

  EXPECT_EQ(input.x(), output.x());
  EXPECT_EQ(input.y(), output.y());
  EXPECT_EQ(input.screen_id(), output.screen_id());
}

struct KeyEventParams {
  bool pressed;
  bool caps_lock_state;
  bool num_lock_state;
};

class KeyEventTest : public testing::TestWithParam<KeyEventParams> {};

TEST_P(KeyEventTest, RoundTrip) {
  const auto& params = GetParam();
  protocol::KeyEvent input;
  input.set_pressed(params.pressed);
  input.set_usb_keycode(0x070004);
  input.set_lock_states(1);
  input.set_caps_lock_state(params.caps_lock_state);
  input.set_num_lock_state(params.num_lock_state);

  protocol::KeyEvent output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::KeyEvent>(input, output));

  EXPECT_EQ(input.pressed(), output.pressed());
  EXPECT_EQ(input.usb_keycode(), output.usb_keycode());
  EXPECT_EQ(input.lock_states(), output.lock_states());
  EXPECT_EQ(input.caps_lock_state(), output.caps_lock_state());
  EXPECT_EQ(input.num_lock_state(), output.num_lock_state());
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         KeyEventTest,
                         testing::Values(KeyEventParams{false, false, false},
                                         KeyEventParams{false, false, true},
                                         KeyEventParams{false, true, false},
                                         KeyEventParams{false, true, true},
                                         KeyEventParams{true, false, false},
                                         KeyEventParams{true, false, true},
                                         KeyEventParams{true, true, false},
                                         KeyEventParams{true, true, true}));

class MouseEventTest : public testing::TestWithParam<bool> {};

TEST_P(MouseEventTest, RoundTrip) {
  bool button_down = GetParam();
  protocol::MouseEvent input;
  input.set_x(100);
  input.set_y(200);
  input.set_button(protocol::MouseEvent::BUTTON_LEFT);
  input.set_button_down(button_down);
  input.set_wheel_delta_x(1.5f);
  input.set_wheel_delta_y(2.5f);
  input.set_wheel_ticks_x(1.0f);
  input.set_wheel_ticks_y(2.0f);
  input.set_delta_x(5);
  input.set_delta_y(10);
  input.mutable_fractional_coordinate()->set_x(0.5f);
  input.mutable_fractional_coordinate()->set_y(0.6f);
  input.mutable_fractional_coordinate()->set_screen_id(1);

  protocol::MouseEvent output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::MouseEvent>(input, output));

  EXPECT_EQ(input.x(), output.x());
  EXPECT_EQ(input.y(), output.y());
  EXPECT_EQ(input.button(), output.button());
  EXPECT_EQ(input.button_down(), output.button_down());
  EXPECT_EQ(input.wheel_delta_x(), output.wheel_delta_x());
  EXPECT_EQ(input.wheel_delta_y(), output.wheel_delta_y());
  EXPECT_EQ(input.wheel_ticks_x(), output.wheel_ticks_x());
  EXPECT_EQ(input.wheel_ticks_y(), output.wheel_ticks_y());
  EXPECT_EQ(input.delta_x(), output.delta_x());
  EXPECT_EQ(input.delta_y(), output.delta_y());
  EXPECT_EQ(input.fractional_coordinate().x(),
            output.fractional_coordinate().x());
  EXPECT_EQ(input.fractional_coordinate().y(),
            output.fractional_coordinate().y());
  EXPECT_EQ(input.fractional_coordinate().screen_id(),
            output.fractional_coordinate().screen_id());
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         MouseEventTest,
                         testing::Bool());

class VideoLayoutTest : public testing::TestWithParam<bool> {};

TEST_P(VideoLayoutTest, RoundTrip) {
  bool supports_full_desktop_capture = GetParam();
  protocol::VideoLayout input;
  input.set_supports_full_desktop_capture(supports_full_desktop_capture);
  input.set_primary_screen_id(123);
  input.set_pixel_type(protocol::VideoLayout::LOGICAL);
  auto* track = input.add_video_track();
  track->set_screen_id(456);
  track->set_media_stream_id("test_stream");
  track->set_position_x(10);
  track->set_position_y(20);
  track->set_width(100);
  track->set_height(200);
  track->set_x_dpi(96);
  track->set_y_dpi(96);
  track->set_display_name("test_display");

  protocol::VideoLayout output;
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoLayout>(input, output));

  EXPECT_EQ(input.supports_full_desktop_capture(),
            output.supports_full_desktop_capture());
  EXPECT_EQ(input.primary_screen_id(), output.primary_screen_id());
  EXPECT_EQ(input.pixel_type(), output.pixel_type());
  EXPECT_EQ(input.video_track_size(), output.video_track_size());
  EXPECT_EQ(input.video_track(0).screen_id(),
            output.video_track(0).screen_id());
  EXPECT_EQ(input.video_track(0).media_stream_id(),
            output.video_track(0).media_stream_id());
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         VideoLayoutTest,
                         testing::Bool());

class MicrophoneControlTest : public testing::TestWithParam<bool> {};

TEST_P(MicrophoneControlTest, RoundTrip) {
  bool enabled = GetParam();
  protocol::MicrophoneControl input;
  input.set_enable(enabled);

  protocol::MicrophoneControl output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::MicrophoneControl>(
      input, output));

  EXPECT_EQ(input.enable(), output.enable());
}

INSTANTIATE_TEST_SUITE_P(RemotingMojomTraitsTest,
                         MicrophoneControlTest,
                         testing::Bool());

TEST(RemotingMojomTraitsTest, IpcFifoBufferReader) {
  std::unique_ptr<IpcFifoBufferWriter> writer;
  std::unique_ptr<IpcFifoBufferReader> reader;
  ASSERT_TRUE(CreateIpcFifoBuffer(1024, writer, reader));

  // Write some data to verify SPSC round-trip through serialization.
  std::vector<uint8_t> data = {1, 2, 3, 4};
  EXPECT_EQ(writer->Write(data), FifoBufferWriter::Result::kSuccess);

  std::unique_ptr<IpcFifoBufferReader> output;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::IpcFifoBufferReader>(
      reader, output));

  ASSERT_TRUE(output);
  EXPECT_EQ(output->GetBufferedBytes(), 4u);

  std::vector<uint8_t> read_data(4);
  EXPECT_EQ(output->Read(read_data), 4u);
  EXPECT_EQ(read_data, data);
}

}  // namespace remoting

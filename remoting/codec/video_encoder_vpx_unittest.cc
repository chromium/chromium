// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/codec/video_encoder_vpx.h"

#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "remoting/codec/codec_test.h"
#include "remoting/proto/video.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

// xRGB pixel colors for use by tests.
const uint32_t kBlueColor = 0x0000ff;
const uint32_t kGreenColor = 0x00ff00;

// Creates a frame stippled between blue and red pixels, which is useful for
// lossy/lossless color tests. By default all pixels in the frame are included
// in the updated_region().
static std::unique_ptr<webrtc::DesktopFrame> CreateTestFrame(
    const webrtc::DesktopSize& frame_size) {
  std::unique_ptr<webrtc::DesktopFrame> frame(
      new webrtc::BasicDesktopFrame(frame_size));
  for (int x = 0; x < frame_size.width(); ++x) {
    for (int y = 0; y < frame_size.height(); ++y) {
      uint8_t* pixel_u8 = frame->data() + (y * frame->stride()) +
                          (x * webrtc::DesktopFrame::kBytesPerPixel);
      *(reinterpret_cast<uint32_t*>(pixel_u8)) =
          ((x + y) & 1) ? kGreenColor : kBlueColor;
    }
  }
  frame->mutable_updated_region()->SetRect(
      webrtc::DesktopRect::MakeSize(frame_size));
  return frame;
}

TEST(VideoEncoderVpxTest, Vp8) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());
  TestVideoEncoder(encoder.get(), false);
}

TEST(VideoEncoderVpxTest, Vp9) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());
  // VP9 encoder defaults to lossy (I420) color.
  TestVideoEncoder(encoder.get(), false);
}

// Test that the VP9 encoder can switch between lossy & lossless color.
TEST(VideoEncoderVpxTest, Vp9LossyColorSwitching) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());

  webrtc::DesktopSize frame_size(100, 100);
  std::unique_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));

  // Lossy encode the first frame.
  encoder->SetLosslessColor(false);
  std::unique_ptr<VideoPacket> lossy_packet = encoder->Encode(*frame);

  // Lossless encode the second frame.
  encoder->SetLosslessColor(true);
  std::unique_ptr<VideoPacket> lossless_packet = encoder->Encode(*frame);

  // Lossy encode one more frame.
  encoder->SetLosslessColor(false);
  lossy_packet = encoder->Encode(*frame);
}

// Test that the VP8 encoder ignores lossless modes without crashing.
TEST(VideoEncoderVpxTest, Vp8IgnoreLossy) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  webrtc::DesktopSize frame_size(100, 100);
  std::unique_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));

  // Encode a frame, to give the encoder a chance to crash if misconfigured.
  encoder->SetLosslessColor(true);
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that calling Encode with a larger frame size than the initial one
// does not cause VP8 to crash.
TEST(VideoEncoderVpxTest, Vp8SizeChangeNoCrash) {
  webrtc::DesktopSize frame_size(100, 100);

  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  // Create first frame & encode it.
  std::unique_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);

  // Double the size of the frame, and updated region, and encode again.
  frame_size.set(frame_size.width() * 2, frame_size.height() * 2);
  frame = CreateTestFrame(frame_size);
  packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that calling Encode with a larger frame size than the initial one
// does not cause VP9 to crash.
TEST(VideoEncoderVpxTest, Vp9SizeChangeNoCrash) {
  webrtc::DesktopSize frame_size(100, 100);

  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());

  // Create first frame & encode it.
  std::unique_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);

  // Double the size of the frame, and updated region, and encode again.
  frame_size.set(frame_size.width() * 2, frame_size.height() * 2);
  frame = CreateTestFrame(frame_size);
  packet = encoder->Encode(*frame);
  EXPECT_TRUE(packet);
}

// Test that the DPI information is correctly propagated from the
// webrtc::DesktopFrame to the VideoPacket.
TEST(VideoEncoderVpxTest, DpiPropagation) {
  webrtc::DesktopSize frame_size(32, 32);

  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());

  std::unique_ptr<webrtc::DesktopFrame> frame(CreateTestFrame(frame_size));
  frame->set_dpi(webrtc::DesktopVector(96, 97));
  std::unique_ptr<VideoPacket> packet = encoder->Encode(*frame);
  EXPECT_EQ(packet->format().x_dpi(), 96);
  EXPECT_EQ(packet->format().y_dpi(), 97);
}

TEST(VideoEncoderVpxTest, Vp8EncodeUnchangedFrame) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP8());
  TestVideoEncoderEmptyFrames(encoder.get(), 0);
}

TEST(VideoEncoderVpxTest, Vp9LossyUnchangedFrame) {
  std::unique_ptr<VideoEncoderVpx> encoder(VideoEncoderVpx::CreateForVP9());
  // Expect that VP9+CR should generate no more than 10 top-off frames
  // per cycle, and take no more than 2 cycles to top-off.
  TestVideoEncoderEmptyFrames(encoder.get(), 20);
}

}  // namespace remoting

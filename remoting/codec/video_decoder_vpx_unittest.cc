// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_decoder_vpx.h"

#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_encoder_vpx.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

class VideoDecoderVpxTest : public testing::Test {
 protected:
  std::unique_ptr<VideoEncoderVpx> encoder_;
  std::unique_ptr<VideoDecoderVpx> decoder_;

  VideoDecoderVpxTest()
      : encoder_(VideoEncoderVpx::CreateForVP8()),
        decoder_(VideoDecoderVpx::CreateForVP8()) {}

  void TestGradient(int screen_width,
                    int screen_height,
                    double max_error_limit,
                    double mean_error_limit) {
    TestVideoEncoderDecoderGradient(
        encoder_.get(), decoder_.get(),
        webrtc::DesktopSize(screen_width, screen_height), max_error_limit,
        mean_error_limit);
  }
};

class VideoDecoderVp8Test : public VideoDecoderVpxTest {
 protected:
  VideoDecoderVp8Test() {
    encoder_ = VideoEncoderVpx::CreateForVP8();
    decoder_ = VideoDecoderVpx::CreateForVP8();
  }
};

class VideoDecoderVp9Test : public VideoDecoderVpxTest {
 protected:
  VideoDecoderVp9Test() {
    encoder_ = VideoEncoderVpx::CreateForVP9();
    decoder_ = VideoDecoderVpx::CreateForVP9();
  }
};

}  // namespace

//
// Test the VP8 codec.
//

TEST_F(VideoDecoderVp8Test, VideoEncodeAndDecode) {
  TestVideoEncoderDecoder(encoder_.get(), decoder_.get(), false);
}

// Check that encoding and decoding a particular frame doesn't change the
// frame too much. The frame used is a gradient, which does not contain sharp
// transitions, so encoding lossiness should not be too high.
TEST_F(VideoDecoderVp8Test, Gradient) {
  TestGradient(320, 240, 0.04, 0.02);
}

//
// Test the VP9 codec.
//

TEST_F(VideoDecoderVp9Test, VideoEncodeAndDecode) {
  TestVideoEncoderDecoder(encoder_.get(), decoder_.get(), false);
}

// Check that encoding and decoding a particular frame doesn't change the
// frame too much. The frame used is a gradient, which does not contain sharp
// transitions, so encoding lossiness should not be too high.
TEST_F(VideoDecoderVp9Test, Gradient) {
  TestGradient(320, 240, 0.04, 0.02);
}

}  // namespace remoting

// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/video_encoder_verbatim.h"

#include "remoting/codec/codec_test.h"
#include "remoting/codec/video_decoder_verbatim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(VideoEncoderVerbatimTest, TestVideoEncoder) {
  std::unique_ptr<VideoEncoderVerbatim> encoder(new VideoEncoderVerbatim());
  TestVideoEncoder(encoder.get(), true);
}

TEST(VideoEncoderVerbatimTest, EncodeAndDecode) {
  std::unique_ptr<VideoEncoderVerbatim> encoder(new VideoEncoderVerbatim());
  std::unique_ptr<VideoDecoderVerbatim> decoder(new VideoDecoderVerbatim());
  TestVideoEncoderDecoder(encoder.get(), decoder.get(), true);
}

TEST(VideoEncoderVerbatimTest, EncodeUnchangedFrame) {
  std::unique_ptr<VideoEncoderVerbatim> encoder(new VideoEncoderVerbatim());
  TestVideoEncoderEmptyFrames(encoder.get(), 0);
}

}  // namespace remoting

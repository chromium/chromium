// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_MOCK_VIDEO_ENCODER_H_
#define MEDIA_CAST_TEST_MOCK_VIDEO_ENCODER_H_

#include "media/cast/encoding/video_encoder.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media::cast {

class MockVideoEncoder : public VideoEncoder {
 public:
  MockVideoEncoder();

  // Not movable or copyable due to mock functionality.
  MockVideoEncoder(MockVideoEncoder&&) = delete;
  MockVideoEncoder& operator=(MockVideoEncoder&&) = delete;
  MockVideoEncoder(const MockVideoEncoder&) = delete;
  MockVideoEncoder& operator=(const MockVideoEncoder&) = delete;
  ~MockVideoEncoder() override;

  // If true is returned, the Encoder has accepted the request and will process
  // it asynchronously, running `frame_encoded_callback` on the MAIN
  // CastEnvironment thread with the result.  If false is returned, nothing
  // happens and the callback will not be run.
  MOCK_METHOD(bool,
              EncodeVideoFrame,
              (scoped_refptr<media::VideoFrame> video_frame,
               base::TimeTicks reference_time,
               FrameEncodedCallback frame_encoded_callback),
              (override));

  // Inform the encoder about the new target bit rate.
  MOCK_METHOD(void, SetBitRate, (int new_bit_rate), (override));

  // Inform the encoder to encode the next frame as a key frame.
  MOCK_METHOD(void, GenerateKeyFrame, (), (override));
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_MOCK_VIDEO_ENCODER_H_

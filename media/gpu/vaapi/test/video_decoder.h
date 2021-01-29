// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_

namespace media {
namespace vaapi_test {

// VideoDecoder specifies an interface for frame-by-frame libva-based decoding
// with different profiles.
class VideoDecoder {
 public:
  // Result of decoding the current frame.
  enum Result {
    kFailed,
    kOk,
    kEOStream,
  };

  VideoDecoder() = default;
  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;
  virtual ~VideoDecoder() = default;

  // Decodes the next frame in this decoder.
  virtual Result DecodeNextFrame() = 0;

  // Outputs the last decoded frame to a PNG at the given |path|.
  // "Last decoded" includes frames specified not to be shown as well as
  // frames referring to already existing/previously decoded frames.
  // It is therefore possible that the images outputted do not exactly match
  // what is displayed by playing the video stream directly.
  virtual void LastDecodedFrameToPNG(const std::string& path) = 0;

  // Computes the MD5 sum of the last decoded frame and returns a human-readable
  // representation.
  virtual std::string LastDecodedFrameMD5Sum() = 0;

  // Returns whether the last decoded frame was visible.
  virtual bool LastDecodedFrameVisible() = 0;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_

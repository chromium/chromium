// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_

#include "media/video/video_encode_accelerator.h"

#include "media/base/bitstream_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockVideoEncodeAccelerator : public VideoEncodeAccelerator {
 public:
  MockVideoEncodeAccelerator();

  MockVideoEncodeAccelerator(const MockVideoEncodeAccelerator&) = delete;
  MockVideoEncodeAccelerator& operator=(const MockVideoEncodeAccelerator&) =
      delete;

  ~MockVideoEncodeAccelerator() override;

  MOCK_METHOD0(GetSupportedProfiles,
               VideoEncodeAccelerator::SupportedProfiles());
  MOCK_METHOD3(Initialize,
               bool(const VideoEncodeAccelerator::Config& config,
                    VideoEncodeAccelerator::Client* client,
                    std::unique_ptr<MediaLog> media_log));
  MOCK_METHOD2(Encode,
               void(scoped_refptr<VideoFrame> frame, bool force_keyframe));
  MOCK_METHOD1(UseOutputBitstreamBuffer, void(BitstreamBuffer buffer));
  MOCK_METHOD3(RequestEncodingParametersChange,
               void(const Bitrate& bitrate,
                    uint32_t framerate,
                    const std::optional<gfx::Size>& size));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD1(Flush,
               void(media::VideoEncodeAccelerator::FlushCallback callback));

 private:
  void DeleteThis();
};

}  // namespace media

#endif  // MEDIA_VIDEO_MOCK_VIDEO_ENCODE_ACCELERATOR_H_

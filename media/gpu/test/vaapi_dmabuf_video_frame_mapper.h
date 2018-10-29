// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_TEST_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_

#include <memory>

#include "media/gpu/test/video_frame_mapper.h"

namespace media {

class VaapiPictureFactory;
class VaapiWrapper;

namespace test {

// VideoFrameMapper that gains access to the memory referred by DMABuf-backed
// video frame using VA-API.
// VaapiDmaBufVideoFrameMapper creates a new VaapiPicture from the given
// VideoFrame and use a VaapiWrapper to access the memory there.
class VaapiDmaBufVideoFrameMapper : public VideoFrameMapper {
 public:
  ~VaapiDmaBufVideoFrameMapper() override;

  static std::unique_ptr<VideoFrameMapper> Create();

  // VideoFrameMapper override.
  scoped_refptr<VideoFrame> Map(
      scoped_refptr<VideoFrame> video_frame) const override;

 private:
  VaapiDmaBufVideoFrameMapper();

  // Vaapi components for mapping.
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  const std::unique_ptr<VaapiPictureFactory> vaapi_picture_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiDmaBufVideoFrameMapper);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_

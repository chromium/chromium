// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_VAAPI_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_

#include <memory>

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

class VaapiWrapper;

// VideoFrameMapper that provides access to the memory referred by DMABuf-backed
// video frames using VA-API.
// VaapiDmaBufVideoFrameMapper creates a new VaapiPicture from the given
// VideoFrame and use the VaapiWrapper to access the memory there.
class MEDIA_GPU_EXPORT VaapiDmaBufVideoFrameMapper : public VideoFrameMapper {
 public:
  static std::unique_ptr<VideoFrameMapper> Create(VideoPixelFormat format);

  VaapiDmaBufVideoFrameMapper(const VaapiDmaBufVideoFrameMapper&) = delete;
  VaapiDmaBufVideoFrameMapper& operator=(const VaapiDmaBufVideoFrameMapper&) =
      delete;

  ~VaapiDmaBufVideoFrameMapper() override;

  // VideoFrameMapper override.
  scoped_refptr<VideoFrame> MapFrame(
      scoped_refptr<const FrameResource> video_frame,
      int permissions) override;

 private:
  explicit VaapiDmaBufVideoFrameMapper(VideoPixelFormat format);

  SEQUENCE_CHECKER(sequence_checker_);
  // Vaapi components for mapping.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_DMABUF_VIDEO_FRAME_MAPPER_H_

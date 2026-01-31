// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_MAPPABLE_SI_VIDEO_FRAME_MAPPER_H_
#define MEDIA_GPU_CHROMEOS_MAPPABLE_SI_VIDEO_FRAME_MAPPER_H_

#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

// The MappableSIVideoFrameMapper implements functionality to map
// MappableSharedImage-based video frames into memory.
class MEDIA_GPU_EXPORT MappableSIVideoFrameMapper : public VideoFrameMapper {
 public:
  static std::unique_ptr<MappableSIVideoFrameMapper> Create(
      VideoPixelFormat format);

  MappableSIVideoFrameMapper(const MappableSIVideoFrameMapper&) = delete;
  MappableSIVideoFrameMapper& operator=(const MappableSIVideoFrameMapper&) =
      delete;

  ~MappableSIVideoFrameMapper() override = default;

  // VideoFrameMapper implementation.
  scoped_refptr<VideoFrame> MapFrame(
      scoped_refptr<const FrameResource> video_frame,
      int permissions) override;

 private:
  explicit MappableSIVideoFrameMapper(VideoPixelFormat format);
};

}  // namespace media
#endif  // MEDIA_GPU_CHROMEOS_MAPPABLE_SI_VIDEO_FRAME_MAPPER_H_

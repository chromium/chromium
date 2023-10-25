// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_
#define MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_

#include <memory>

#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/video_frame_mapper.h"

namespace media {

// A factory function for VideoFrameMapper.
// The appropriate VideoFrameMapper is a platform-dependent.
class MEDIA_GPU_EXPORT VideoFrameMapperFactory {
 public:
  // Tries to create a VideoFrameMapper suitable for mapping VideoFrames
  // described by |format| and |storage_type|. If
  // |must_support_intel_media_compressed_buffers| is true, the returned
  // VideoFrameMapper (if any) can map Intel media compressed VideoFrames.
  // Returns nullptr if no suitable VideoFrameMapper can be created.
  static std::unique_ptr<VideoFrameMapper> CreateMapper(
      VideoPixelFormat format,
      VideoFrame::StorageType storage_type,
      bool must_support_intel_media_compressed_buffers);

  // Tries to create a VideoFrameMapper suitable for mapping VideoFrames
  // described by |format| and |storage_type|. If
  // |must_support_intel_media_compressed_buffers| is true, the returned
  // VideoFrameMapper (if any) can map Intel media compressed VideoFrames. If
  // |force_linear_buffer_mapper| is true and the |storage_type| is not
  // VideoFrame::STORAGE_GPU_MEMORY_BUFFER, the returned VideoFrameMapper (if
  // any) won't attempt to linearize the buffer. Returns nullptr if no suitable
  // VideoFrameMapper can be created.
  static std::unique_ptr<VideoFrameMapper> CreateMapper(
      VideoPixelFormat format,
      VideoFrame::StorageType storage_type,
      bool force_linear_buffer_mapper,
      bool must_support_intel_media_compressed_buffers);
};

}  // namespace media

#endif  // MEDIA_GPU_VIDEO_FRAME_MAPPER_FACTORY_H_

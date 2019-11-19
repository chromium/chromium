// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper_factory.h"

#include "build/build_config.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/linux/generic_dmabuf_video_frame_mapper.h"
#include "media/gpu/linux/gpu_memory_buffer_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type) {
#if BUILDFLAG(USE_VAAPI)
  return CreateMapper(format, storage_type, false);
#else
  return CreateMapper(format, storage_type, true);
#endif  // BUILDFLAG(USE_VAAPI)
}

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type,
    bool linear_buffer_mapper) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER)
    return GpuMemoryBufferVideoFrameMapper::Create(format);

  if (linear_buffer_mapper)
    return GenericDmaBufVideoFrameMapper::Create(format);
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_VAAPI)
  return VaapiDmaBufVideoFrameMapper::Create(format);
#endif  // BUILDFLAG(USE_VAAPI)

  return nullptr;
}

}  // namespace media

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper_factory.h"

#include "build/build_config.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"
#include "media/gpu/chromeos/gpu_memory_buffer_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type,
    bool must_support_intel_media_compressed_buffers) {
#if BUILDFLAG(USE_VAAPI)
  // TODO(b/307769458): the fake libva driver does not currently implement the
  // VAImage functionality necessary to use the VaapiDmaBufVideoFrameMapper. For
  // now, as a workaround, let's assume that when using that driver, buffers are
  // already linear.
  const bool force_linear_buffer_mapper =
      (VaapiWrapper::GetImplementationType() ==
       VAImplementation::kChromiumFakeDriver);
  return CreateMapper(format, storage_type, force_linear_buffer_mapper,
                      must_support_intel_media_compressed_buffers);
#else
  return CreateMapper(format, storage_type, true,
                      must_support_intel_media_compressed_buffers);
#endif  // BUILDFLAG(USE_VAAPI)
}

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type,
    bool force_linear_buffer_mapper,
    bool must_support_intel_media_compressed_buffers) {
  if (!must_support_intel_media_compressed_buffers) {
    if (storage_type == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
      return GpuMemoryBufferVideoFrameMapper::Create(format);
    }

    if (force_linear_buffer_mapper) {
      return GenericDmaBufVideoFrameMapper::Create(format);
    }
  } else if (force_linear_buffer_mapper ||
             storage_type != VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // We currently only support Intel media compressed VideoFrames if they are
    // backed by a GpuMemoryBuffer. Additionally, we don't currently support
    // mapping Intel media compressed buffers without linearizing them.
    return nullptr;
  }

#if BUILDFLAG(USE_VAAPI)
  return VaapiDmaBufVideoFrameMapper::Create(format);
#else
  return nullptr;
#endif  // BUILDFLAG(USE_VAAPI)
}

}  // namespace media

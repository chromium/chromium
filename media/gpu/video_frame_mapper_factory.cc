// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper_factory.h"

#include "build/build_config.h"
#include "media/base/decoder.h"
#include "media/gpu/buildflags.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(USE_LINUX_VIDEO_ACCELERATION)
// These includes are used for non-ChromeOS platforms as well.
#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"
#include "media/gpu/chromeos/mappable_si_video_frame_mapper.h"
#endif

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type) {
#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  // VA-API uses the zero-copy non-linear path, while V4L2 uses linear.
  const bool linear = ActiveLinuxVideoDecoderType() == VideoDecoderType::kV4L2;
  return CreateMapper(format, storage_type, linear);
#endif
}

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    VideoFrame::StorageType storage_type,
    bool force_linear_buffer_mapper) {
  if (storage_type == VideoFrame::STORAGE_MAPPABLE_SHARED_IMAGE) {
    return MappableSIVideoFrameMapper::Create(format);
  }

  if (force_linear_buffer_mapper) {
    return GenericDmaBufVideoFrameMapper::Create(format);
  }

#if BUILDFLAG(USE_VAAPI)
  // VaapiVideoDecoder zero-copy-imports VideoFrames into the GPU. The
  // |force_linear_buffer_mapper| early-return above already handled the
  // libyuv conversion path; here we always take the zero-copy path.
  return VaapiDmaBufVideoFrameMapper::Create(format);
#else
  // No zero-copy backend is compiled in. The caller asked for zero-copy
  // (otherwise the early-return for |force_linear_buffer_mapper| would have
  // fired); return nullptr so they can fall back explicitly.
  return nullptr;
#endif  // BUILDFLAG(USE_VAAPI)
}

}  // namespace media

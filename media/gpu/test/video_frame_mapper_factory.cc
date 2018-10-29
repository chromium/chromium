// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_mapper_factory.h"

#include "media/gpu/buildflags.h"

#if defined(OS_CHROMEOS)
#include "media/gpu/test/generic_dmabuf_video_frame_mapper.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/test/vaapi_dmabuf_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

#endif  // defined(OS_CHROMEOS)

namespace media {
namespace test {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper() {
#if defined(OS_CHROMEOS)

#if BUILDFLAG(USE_VAAPI)
  return VaapiDmaBufVideoFrameMapper::Create();
#else
  return std::make_unique<GenericDmaBufVideoFrameMapper>();
#endif  // BUILDFLAG(USE_VAAPI)

#endif  // defined(OS_CHROMEOS)

  NOTREACHED();
  return nullptr;
}

}  // namespace test
}  // namespace media

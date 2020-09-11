// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_
#define MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>

#include "base/optional.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gl/hdr_metadata.h"

namespace media {

MEDIA_GPU_EXPORT CFMutableDictionaryRef
CreateFormatExtensions(CMVideoCodecType codec_type,
                       VideoCodecProfile profile,
                       const VideoColorSpace& color_space,
                       base::Optional<gl::HDRMetadata> hdr_metadata);

MEDIA_GPU_EXPORT gfx::ColorSpace GetImageBufferColorSpace(
    CVImageBufferRef image_buffer);

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

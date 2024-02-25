// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_
#define MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>

#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

MEDIA_GPU_EXPORT base::apple::ScopedCFTypeRef<CFDictionaryRef>
CreateFormatExtensions(CMVideoCodecType codec_type,
                       VideoCodecProfile profile,
                       int bit_depth,
                       const VideoColorSpace& color_space,
                       std::optional<gfx::HDRMetadata> hdr_metadata,
                       std::optional<base::span<const uint8_t>> csd_box);

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

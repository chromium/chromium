// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_
#define MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>

#include "base/mac/scoped_cftyperef.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

MEDIA_GPU_EXPORT base::ScopedCFTypeRef<CFDictionaryRef> CreateFormatExtensions(
    CMVideoCodecType codec_type,
    VideoCodecProfile profile,
    const VideoColorSpace& color_space,
    absl::optional<gfx::HDRMetadata> hdr_metadata);

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_CONFIG_UTIL_H_

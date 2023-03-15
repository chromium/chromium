// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_
#define MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

MEDIA_EXPORT gfx::ColorSpace GetImageBufferColorSpace(
    CVImageBufferRef image_buffer);

MEDIA_EXPORT gfx::ColorSpace GetFormatDescriptionColorSpace(
    CMFormatDescriptionRef format_description);

// Converts a gfx::ColorSpace to individual kCVImageBuffer* keys.
MEDIA_EXPORT bool GetImageBufferColorValues(const gfx::ColorSpace& color_space,
                                            CFStringRef* out_primaries,
                                            CFStringRef* out_transfer,
                                            CFStringRef* out_matrix);

}  // namespace media

#endif  // MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_

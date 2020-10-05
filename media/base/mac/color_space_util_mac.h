// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_
#define MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

MEDIA_EXPORT gfx::ColorSpace GetImageBufferColorSpace(
    CVImageBufferRef image_buffer);

MEDIA_EXPORT gfx::ColorSpace GetFormatDescriptionColorSpace(
    CMFormatDescriptionRef format_description) API_AVAILABLE(macos(10.11));

}  // namespace media

#endif  // MEDIA_BASE_MAC_COLOR_SPACE_UTIL_MAC_H_

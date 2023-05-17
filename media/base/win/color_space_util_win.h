// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_
#define MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_

class IMFMediaType;

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

MEDIA_EXPORT gfx::ColorSpace GetMediaTypeColorSpace(IMFMediaType* media_type);

}  // namespace media

#endif  // MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_
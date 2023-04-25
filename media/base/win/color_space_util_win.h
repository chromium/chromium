// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_
#define MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_

#include <mfidl.h>

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

MEDIA_EXPORT gfx::ColorSpace GetMediaTypeColorSpace(IMFMediaType* media_type);

// Converts a gfx::ColorSpace to individual MFVideo* keys.
MEDIA_EXPORT void GetMediaTypeColorValues(const gfx::ColorSpace& color_space,
                                          MFVideoPrimaries* out_primaries,
                                          MFVideoTransferFunction* out_transfer,
                                          MFVideoTransferMatrix* out_matrix,
                                          MFNominalRange* out_range);

}  // namespace media

#endif  // MEDIA_BASE_WIN_COLOR_SPACE_UTIL_WIN_H_
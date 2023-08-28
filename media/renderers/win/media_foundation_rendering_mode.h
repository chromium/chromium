// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERING_MODE_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERING_MODE_H_

#include "media/base/media_export.h"

#include <ostream>

namespace media {

// This C++ enum is the equivalent to mojom::MediaFoundationRenderingMode
enum class MediaFoundationRenderingMode : int32_t {
  DirectComposition = 0,
  FrameServer = 1,
  kMaxValue = 1,
};

MEDIA_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const MediaFoundationRenderingMode& render_mode);

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_RENDERING_MODE_H_

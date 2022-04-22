// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

class LocalFrame;

inline AutoDarkMode PaintAutoDarkMode(const ComputedStyle& style,
                                      DarkModeFilter::ElementRole role) {
  return AutoDarkMode(role, style.ForceDark());
}

inline AutoDarkMode PaintAutoDarkMode(DarkModeFilter::ElementRole role,
                                      bool auto_dark_mode_enabled) {
  return AutoDarkMode(role, auto_dark_mode_enabled);
}

class ImageClassifierHelper {
 public:
  CORE_EXPORT static ImageAutoDarkMode GetImageAutoDarkMode(
      LocalFrame& local_frame,
      const ComputedStyle& style,
      const gfx::RectF& dest_rect,
      const gfx::RectF& src_rect,
      DarkModeFilter::ElementRole role =
          DarkModeFilter::ElementRole::kBackground);

  CORE_EXPORT static DarkModeFilter::ImageType GetImageTypeForTesting(
      LocalFrame& local_frame,
      const gfx::RectF& dest_rect,
      const gfx::RectF& src_rect);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

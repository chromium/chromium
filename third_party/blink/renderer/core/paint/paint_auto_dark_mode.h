// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

inline AutoDarkMode PaintAutoDarkMode(const ComputedStyle& style,
                                      DarkModeFilter::ElementRole role) {
  return AutoDarkMode(role, style.ForceDark());
}

inline AutoDarkMode PaintAutoDarkMode(DarkModeFilter::ElementRole role,
                                      bool auto_dark_mode_enabled) {
  return AutoDarkMode(role, auto_dark_mode_enabled);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

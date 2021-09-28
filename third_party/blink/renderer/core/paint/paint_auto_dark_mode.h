// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

namespace blink {

// TODO(crbug.com/1224806): Remove this and only rely on the ComputedStyle of
// each object.
inline bool AutoDarkModeEnabled(const Document& document) {
  if (!document.GetSettings()->GetForceDarkModeEnabled())
    return false;
  if (document.GetLayoutView() &&
      document.GetLayoutView()->StyleRef().DarkColorScheme()) {
    return false;
  }
  return true;
}

// TODO(crbug.com/1224806): Remove this and only rely on the |ComputedStyle|
// version of this function, rather than relying on |Document|.
inline AutoDarkMode PaintAutoDarkMode(const ComputedStyle& style,
                                      const Document& document,
                                      DarkModeFilter::ElementRole role) {
  // TODO(crbug.com/1224806): Use |style.ForceDark()| here.
  return AutoDarkMode(
      role, AutoDarkModeEnabled(document) && !style.DisableForceDark());
}

inline AutoDarkMode PaintAutoDarkMode(const ComputedStyle& style,
                                      DarkModeFilter::ElementRole role) {
  // TODO(crbug.com/1224806): Use |style.ForceDark()| here.
  // TODO(crbug.com/1224806): Without access to the global setting, this will
  // incorrectly disable auto dark mode.
  bool auto_dark_mode_enabled = false;
  return AutoDarkMode(role,
                      auto_dark_mode_enabled && !style.DisableForceDark());
}

inline AutoDarkMode PaintAutoDarkMode(DarkModeFilter::ElementRole role,
                                      bool auto_dark_mode_enabled) {
  return AutoDarkMode(role, auto_dark_mode_enabled);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_AUTO_DARK_MODE_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_

namespace blink {

// New variables added to this struct should be considered in
// third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h
struct DarkModeSettings {
  // Foreground colors with brightness below this threshold will be inverted,
  // and above it will be left as in the original, non-dark-mode page.  Set to
  // 255 to always invert foreground color or to 0 to never invert text color.
  int foreground_brightness_threshold = 255;

  // Background elements with brightness above this threshold will be inverted,
  // and below it will be left as in the original, non-dark-mode page.  Set to
  // 256 to never invert the color or to 0 to always invert it.
  //
  // Warning: This behavior is the opposite of foreground_brightness_threshold!
  int background_brightness_threshold = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_

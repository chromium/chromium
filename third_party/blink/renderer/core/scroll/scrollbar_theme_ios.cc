// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"

#include "third_party/blink/renderer/core/scroll/scrollbar_theme_overlay_mobile.h"

namespace blink {

ScrollbarTheme& ScrollbarTheme::NativeTheme() {
  return ScrollbarThemeOverlayMobile::GetInstance();
}

}  // namespace blink

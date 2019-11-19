// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/win/dwrite_font_format_support.h"

#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkFontStyle.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

bool DWriteVersionSupportsVariations() {
  // We're instantiating a default typeface. The usage of legacyMakeTypeface()
  // is intentional here to access a basic default font. Its implementation will
  // ultimately use the first font face from the first family in the system font
  // collection. Use this probe type face to ask Skia for the variation design
  // position. Internally, Skia then tests whether the DWrite interfaces for
  // accessing variable font information are available, in other words, if
  // QueryInterface for IDWriteFontFace5 succeeds. If it doesn't it returns -1
  // and we know DWrite on this system does not support OpenType variations. If
  // the response is 0 or larger, it means, DWrite was able to determine if this
  // is a variable font or not and Variations are supported.
  static bool variations_supported = []() {
    auto fm(SkFontMgr::RefDefault());
    sk_sp<SkTypeface> probe_typeface =
        fm->legacyMakeTypeface(nullptr, SkFontStyle());
    int variation_design_position_result =
        probe_typeface->getVariationDesignPosition(nullptr, 0);

    return variation_design_position_result > -1;
  }();
  return variations_supported;
}

}  // namespace blink

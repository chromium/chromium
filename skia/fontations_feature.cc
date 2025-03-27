// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/fontations_feature.h"

namespace skia {
// Instantiate system fonts on Linux with Fontations, affects
// SkFontMgr instantiation in skia/ext/font_utils.cc
BASE_FEATURE(kFontationsLinuxSystemFonts,
             "FontationsLinuxSystemFonts",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Instantiate system fonts on Android with Fontations, affects
// SkFontMgr instantiation in skia/ext/font_utils.cc
BASE_FEATURE(kFontationsAndroidSystemFonts,
             "FontationsAndroidSystemFonts",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace skia

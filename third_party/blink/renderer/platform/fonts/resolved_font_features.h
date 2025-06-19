// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_

#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontFeatureSettings;
using ResolvedFontFeatures = Vector<FontFeatureValue>;

PLATFORM_EXPORT ResolvedFontFeatures ResolveFontFeatureSettingsDescriptor(
    const FontFeatureSettings* existing_features_settings,
    const FontFeatureSettings* new_settings);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_RESOLVED_FONT_FEATURES_H_

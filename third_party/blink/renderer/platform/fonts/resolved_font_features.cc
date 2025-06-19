// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/resolved_font_features.h"

#include <unordered_set>

#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/shaping/font_features.h"

namespace blink {

PLATFORM_EXPORT ResolvedFontFeatures ResolveFontFeatureSettingsDescriptor(
    const FontFeatureSettings* existing_features_settings,
    const FontFeatureSettings* new_settings) {
  ResolvedFontFeatures resolved_font_features;
  if (!new_settings) {
    return resolved_font_features;
  }

  std::unordered_set<uint32_t> existing_tags;
  if (existing_features_settings) {
    for (const FontFeature& feature : *existing_features_settings) {
      existing_tags.insert(feature.Tag());
    }
  }
  for (const FontFeature& feature : *new_settings) {
    // As per the CSS Fonts Level 3 specification
    // (https://www.w3.org/TR/css-fonts-3/#feature-precedence), CSS properties
    // take precedence over @font-face descriptors when both define the same
    // feature, ensuring authors maintain full control over font behavior.
    // To implement this priority in code, we first verify if the feature
    // already exists in existing_settings (representing CSS-defined settings).
    // If found, we skip adding it from @font-face to prevent overriding the
    // CSS-defined value.
    if (existing_tags.find(feature.Tag()) == existing_tags.end()) {
      resolved_font_features.emplace_back(FontFeatureValue{
          feature.Tag(), static_cast<uint32_t>(feature.Value())});
    }
  }
  return resolved_font_features;
}

}  // namespace blink

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/preferences/preference_overrides.h"

#include "third_party/blink/renderer/core/css/media_feature_overrides.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/core/css/media_values.h"

namespace blink {

void PreferenceOverrides::SetOverride(const AtomicString& feature,
                                      const String& value_string,
                                      const Document* document) {
  MediaQueryExpValue value = MediaFeatureOverrides::ParseMediaQueryValue(
      feature, value_string, document);

  if (feature == media_feature_names::kPrefersColorSchemeMediaFeature) {
    preferred_color_scheme_ =
        MediaFeatureOverrides::ConvertPreferredColorScheme(value);
  } else if (feature == media_feature_names::kPrefersContrastMediaFeature) {
    preferred_contrast_ =
        MediaFeatureOverrides::ConvertPreferredContrast(value);
  } else if (feature ==
             media_feature_names::kPrefersReducedMotionMediaFeature) {
    prefers_reduced_motion_ =
        MediaFeatureOverrides::ConvertPrefersReducedMotion(value);
  } else if (feature == media_feature_names::kPrefersReducedDataMediaFeature) {
    prefers_reduced_data_ =
        MediaFeatureOverrides::ConvertPrefersReducedData(value);
  } else if (feature ==
             media_feature_names::kPrefersReducedTransparencyMediaFeature) {
    prefers_reduced_transparency_ =
        MediaFeatureOverrides::ConvertPrefersReducedTransparency(value);
  }
}

}  // namespace blink

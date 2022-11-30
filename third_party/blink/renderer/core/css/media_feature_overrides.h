// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/css/preferred_contrast.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

enum class ColorSpaceGamut;
enum class ForcedColors;

class CORE_EXPORT MediaFeatureOverrides {
  USING_FAST_MALLOC(MediaFeatureOverrides);

 public:
  void SetOverride(const AtomicString& feature, const String& value_string);

  absl::optional<ColorSpaceGamut> GetColorGamut() const { return color_gamut_; }
  absl::optional<mojom::blink::PreferredColorScheme> GetPreferredColorScheme()
      const {
    return preferred_color_scheme_;
  }
  absl::optional<mojom::blink::PreferredContrast> GetPreferredContrast() const {
    return preferred_contrast_;
  }
  absl::optional<bool> GetPrefersReducedMotion() const {
    return prefers_reduced_motion_;
  }
  absl::optional<bool> GetPrefersReducedData() const {
    return prefers_reduced_data_;
  }
  absl::optional<ForcedColors> GetForcedColors() const {
    return forced_colors_;
  }

 private:
  absl::optional<ColorSpaceGamut> color_gamut_;
  absl::optional<mojom::blink::PreferredColorScheme> preferred_color_scheme_;
  absl::optional<mojom::blink::PreferredContrast> preferred_contrast_;
  absl::optional<bool> prefers_reduced_motion_;
  absl::optional<bool> prefers_reduced_data_;
  absl::optional<ForcedColors> forced_colors_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_

// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TOLERANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TOLERANCE_H_

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

// This class is used to represent the possible values for the
// 'fit-tolerance' property in the CSS Grid Lanes Layout. More information can
// be found here: https://drafts.csswg.org/css-grid-3/#item-slack.
class FitTolerance {
  DISALLOW_NEW();

 public:
  explicit FitTolerance() = default;
  explicit FitTolerance(const Length& length)
      : fit_tolerance_type_(FitToleranceType::kLength), length_(length) {}
  explicit FitTolerance(CSSValueID keyword) {
    if (keyword == CSSValueID::kNormal) {
      fit_tolerance_type_ = FitToleranceType::kNormal;
    } else if (keyword == CSSValueID::kInfinite) {
      fit_tolerance_type_ = FitToleranceType::kInfinite;
    } else {
      NOTREACHED();
    }
  }

  bool IsNormal() const {
    return fit_tolerance_type_ == FitToleranceType::kNormal;
  }
  bool IsInfinite() const {
    return fit_tolerance_type_ == FitToleranceType::kInfinite;
  }
  const Length& GetLength() const {
    DCHECK(!IsNormal() && !IsInfinite());
    return length_;
  }

  bool operator==(const FitTolerance& o) const = default;

 private:
  enum class FitToleranceType { kNormal, kInfinite, kLength };

  FitToleranceType fit_tolerance_type_{FitToleranceType::kNormal};
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FIT_TOLERANCE_H_

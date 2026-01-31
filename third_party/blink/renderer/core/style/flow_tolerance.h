// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FLOW_TOLERANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FLOW_TOLERANCE_H_

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

// This class is used to represent the possible values for the
// 'flow-tolerance' property in the CSS Grid Lanes Layout. More information can
// be found here: https://drafts.csswg.org/css-grid-3/#item-slack.
class FlowTolerance {
  DISALLOW_NEW();

 public:
  explicit FlowTolerance() = default;
  explicit FlowTolerance(const Length& length)
      : flow_tolerance_type_(FlowToleranceType::kLength), length_(length) {}
  explicit FlowTolerance(CSSValueID keyword) {
    if (keyword == CSSValueID::kNormal) {
      flow_tolerance_type_ = FlowToleranceType::kNormal;
    } else if (keyword == CSSValueID::kInfinite) {
      flow_tolerance_type_ = FlowToleranceType::kInfinite;
    } else {
      NOTREACHED();
    }
  }

  bool IsNormal() const {
    return flow_tolerance_type_ == FlowToleranceType::kNormal;
  }
  bool IsInfinite() const {
    return flow_tolerance_type_ == FlowToleranceType::kInfinite;
  }
  const Length& GetLength() const {
    DCHECK(!IsNormal() && !IsInfinite());
    return length_;
  }

  bool operator==(const FlowTolerance& o) const = default;

 private:
  enum class FlowToleranceType { kNormal, kInfinite, kLength };

  FlowToleranceType flow_tolerance_type_{FlowToleranceType::kNormal};
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_FLOW_TOLERANCE_H_

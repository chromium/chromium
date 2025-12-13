// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ITEM_TOLERANCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ITEM_TOLERANCE_H_

#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

// This class is used to represent the possible values for the
// 'item-tolerance' property in the CSS Grid Lanes Layout. More information can
// be found here: https://drafts.csswg.org/css-grid-3/#item-slack.
class ItemTolerance {
  DISALLOW_NEW();

 public:
  explicit ItemTolerance() = default;
  explicit ItemTolerance(const Length& length)
      : item_tolerance_type_(ItemToleranceType::kLength), length_(length) {}
  explicit ItemTolerance(CSSValueID keyword) {
    if (keyword == CSSValueID::kNormal) {
      item_tolerance_type_ = ItemToleranceType::kNormal;
    } else if (keyword == CSSValueID::kInfinite) {
      item_tolerance_type_ = ItemToleranceType::kInfinite;
    } else {
      NOTREACHED();
    }
  }

  bool IsNormal() const {
    return item_tolerance_type_ == ItemToleranceType::kNormal;
  }
  bool IsInfinite() const {
    return item_tolerance_type_ == ItemToleranceType::kInfinite;
  }
  const Length& GetLength() const {
    DCHECK(!IsNormal() && !IsInfinite());
    return length_;
  }

  bool operator==(const ItemTolerance& o) const = default;

 private:
  enum class ItemToleranceType { kNormal, kInfinite, kLength };

  ItemToleranceType item_tolerance_type_{ItemToleranceType::kNormal};
  Length length_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_ITEM_TOLERANCE_H_

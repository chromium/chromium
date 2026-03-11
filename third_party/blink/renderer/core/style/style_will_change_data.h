// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_WILL_CHANGE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_WILL_CHANGE_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// https://drafts.csswg.org/css-will-change-1/#typedef-animateable-feature
struct WillChangeValue {
  DISALLOW_NEW();
  uint16_t property_id : kCSSPropertyIDBitLength =
                             static_cast<uint16_t>(CSSPropertyID::kInvalid);
  uint16_t is_scroll_position : 1 = false;
  uint16_t is_contents : 1 = false;

  bool operator==(const WillChangeValue& other) const {
    return property_id == other.property_id &&
           is_scroll_position == other.is_scroll_position &&
           is_contents == other.is_contents;
  }
};

// Represents the value for CSS "will-change".
// https://drafts.csswg.org/css-will-change-1/#propdef-will-change
class CORE_EXPORT StyleWillChangeData
    : public GarbageCollected<StyleWillChangeData> {
 public:
  StyleWillChangeData(Vector<WillChangeValue>&& values,
                      CSSBitset&& resolved_longhand_ids,
                      bool has_scroll_position_value,
                      bool has_transform_property,
                      bool has_any_transform_property)
      : values(std::move(values)),
        resolved_longhand_ids(std::move(resolved_longhand_ids)),
        has_scroll_position_value(has_scroll_position_value),
        has_transform_property(has_transform_property),
        has_any_transform_property(has_any_transform_property) {}

  bool operator==(const StyleWillChangeData& other) const {
    return values == other.values;
  }

  void Trace(Visitor*) const {}

  // We can't directly store the values in the bitset. We need to preserve the
  // order for getComputedStyle().
  const Vector<WillChangeValue> values;

  // The bitset only contains resolved longhand CSSPropertyID(s). No aliases.
  const CSSBitset resolved_longhand_ids;
  const bool has_scroll_position_value;

  // These two fields are computed during ApplyValue.
  const bool has_transform_property;
  const bool has_any_transform_property;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_WILL_CHANGE_DATA_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_PROPERTY_PRIORITY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_PROPERTY_PRIORITY_H_

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// The values of high priority properties affect the values of low priority
// properties. For example, the value of the high priority property 'font-size'
// decides the pixel value of low priority properties with 'em' units.

// TODO(sashab): Generate the methods in this file.

enum CSSPropertyPriority {
  kHighPropertyPriority,
};

template <CSSPropertyPriority priority>
class CSSPropertyPriorityData {
  STATIC_ONLY(CSSPropertyPriorityData);

 public:
  static constexpr CSSPropertyID First();
  static constexpr CSSPropertyID Last();
  static constexpr bool PropertyHasPriority(CSSPropertyID prop) {
    return First() <= prop && prop <= Last();
  }
};

template <>
constexpr CSSPropertyID
CSSPropertyPriorityData<kHighPropertyPriority>::First() {
  static_assert(
      CSSPropertyID::kColor == firstCSSProperty,
      "CSSPropertyID::kColor should be the first high priority property");
  return CSSPropertyID::kColor;
}

template <>
constexpr CSSPropertyID CSSPropertyPriorityData<kHighPropertyPriority>::Last() {
  static_assert(static_cast<int>(CSSPropertyID::kZoom) ==
                    static_cast<int>(CSSPropertyID::kColor) + 26,
                "CSSPropertyID::kZoom should be the end of the high priority "
                "property range");
  static_assert(static_cast<int>(CSSPropertyID::kWritingMode) ==
                    static_cast<int>(CSSPropertyID::kZoom) - 1,
                "CSSPropertyID::kWritingMode should be immediately before "
                "CSSPropertyID::kZoom");
  return CSSPropertyID::kZoom;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_PROPERTY_PRIORITY_H_

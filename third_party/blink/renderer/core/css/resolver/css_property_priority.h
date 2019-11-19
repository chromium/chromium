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
  kResolveVariables = 0,
  kAnimationPropertyPriority,
  kHighPropertyPriority,
  kLowPropertyPriority,
  kPropertyPriorityCount,
};

template <CSSPropertyPriority priority>
class CSSPropertyPriorityData {
  STATIC_ONLY(CSSPropertyPriorityData);

 public:
  static inline CSSPropertyID First();
  static inline CSSPropertyID Last();
  static inline bool PropertyHasPriority(CSSPropertyID prop) {
    return First() <= prop && prop <= Last();
  }
};

template <>
inline CSSPropertyID CSSPropertyPriorityData<kResolveVariables>::First() {
  static_assert(
      static_cast<int>(CSSPropertyID::kVariable) == kIntFirstCSSProperty - 1,
      "CSSPropertyID::kVariable should be directly before the first CSS "
      "property.");
  return CSSPropertyID::kVariable;
}

template <>
inline CSSPropertyID CSSPropertyPriorityData<kResolveVariables>::Last() {
  return CSSPropertyID::kVariable;
}

template <>
inline CSSPropertyID
CSSPropertyPriorityData<kAnimationPropertyPriority>::First() {
  static_assert(CSSPropertyID::kAnimationDelay == firstCSSProperty,
                "CSSPropertyID::kAnimationDelay should be the first animation "
                "priority property");
  return CSSPropertyID::kAnimationDelay;
}

template <>
inline CSSPropertyID
CSSPropertyPriorityData<kAnimationPropertyPriority>::Last() {
  static_assert(
      static_cast<int>(CSSPropertyID::kTransitionTimingFunction) ==
          static_cast<int>(CSSPropertyID::kAnimationDelay) + 11,
      "CSSPropertyID::kTransitionTimingFunction should be the end of the high "
      "priority property range");
  static_assert(
      static_cast<int>(CSSPropertyID::kColor) ==
          static_cast<int>(CSSPropertyID::kTransitionTimingFunction) + 1,
      "CSSPropertyID::kTransitionTimingFunction should be immediately before "
      "CSSPropertyID::kColor");
  return CSSPropertyID::kTransitionTimingFunction;
}

template <>
inline CSSPropertyID CSSPropertyPriorityData<kHighPropertyPriority>::First() {
  static_assert(
      static_cast<int>(CSSPropertyID::kColor) ==
          static_cast<int>(CSSPropertyID::kTransitionTimingFunction) + 1,
      "CSSPropertyID::kColor should be the first high priority property");
  return CSSPropertyID::kColor;
}

template <>
inline CSSPropertyID CSSPropertyPriorityData<kHighPropertyPriority>::Last() {
  static_assert(static_cast<int>(CSSPropertyID::kZoom) ==
                    static_cast<int>(CSSPropertyID::kColor) + 27,
                "CSSPropertyID::kZoom should be the end of the high priority "
                "property range");
  static_assert(static_cast<int>(CSSPropertyID::kWritingMode) ==
                    static_cast<int>(CSSPropertyID::kZoom) - 1,
                "CSSPropertyID::kWritingMode should be immediately before "
                "CSSPropertyID::kZoom");
  return CSSPropertyID::kZoom;
}

template <>
inline CSSPropertyID CSSPropertyPriorityData<kLowPropertyPriority>::First() {
  static_assert(
      static_cast<int>(CSSPropertyID::kAlignContent) ==
          static_cast<int>(CSSPropertyID::kZoom) + 1,
      "CSSPropertyID::kAlignContent should be the first low priority property");
  return CSSPropertyID::kAlignContent;
}

template <>
inline CSSPropertyID CSSPropertyPriorityData<kLowPropertyPriority>::Last() {
  return static_cast<CSSPropertyID>(lastCSSProperty);
}

inline CSSPropertyPriority PriorityForProperty(CSSPropertyID property) {
  if (CSSPropertyPriorityData<kLowPropertyPriority>::PropertyHasPriority(
          property)) {
    return kLowPropertyPriority;
  }
  if (CSSPropertyPriorityData<kHighPropertyPriority>::PropertyHasPriority(
          property)) {
    return kHighPropertyPriority;
  }
  if (CSSPropertyPriorityData<kAnimationPropertyPriority>::PropertyHasPriority(
          property)) {
    return kAnimationPropertyPriority;
  }
  DCHECK(CSSPropertyPriorityData<kResolveVariables>::PropertyHasPriority(
      property));
  return kResolveVariables;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CSS_PROPERTY_PRIORITY_H_

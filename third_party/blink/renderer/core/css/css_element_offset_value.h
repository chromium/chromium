// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ELEMENT_OFFSET_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ELEMENT_OFFSET_VALUE_H_

#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

// https://drafts.csswg.org/scroll-animations-1/#typedef-element-offset
class CORE_EXPORT CSSElementOffsetValue : public CSSValue {
 public:
  CSSElementOffsetValue(const CSSValue* target,
                        const CSSValue* edge,
                        const CSSValue* threshold);

  const CSSValue* Target() const { return target_; }
  const CSSValue* Edge() const { return edge_; }
  const CSSValue* Threshold() const { return threshold_; }

  String CustomCSSText() const;
  bool Equals(const CSSElementOffsetValue&) const;
  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<const CSSValue> target_;
  Member<const CSSValue> edge_;
  Member<const CSSValue> threshold_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSElementOffsetValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsElementOffsetValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ELEMENT_OFFSET_VALUE_H_

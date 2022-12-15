// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

// https://drafts.csswg.org/scroll-animations-1/#view-notation
class CORE_EXPORT CSSViewValue : public CSSValue {
 public:
  CSSViewValue(const CSSValue* axis);

  const CSSValue* Axis() const { return axis_; }

  String CustomCSSText() const;
  bool Equals(const CSSViewValue&) const;
  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  Member<const CSSValue> axis_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSViewValue> {
  static bool AllowFrom(const CSSValue& value) { return value.IsViewValue(); }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_VALUE_H_

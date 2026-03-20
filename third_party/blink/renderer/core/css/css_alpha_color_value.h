// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ALPHA_COLOR_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ALPHA_COLOR_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// Stores the result of parsing the alpha() function before resolving it into a
// blink::Color. See https://drafts.csswg.org/css-color-5/#relative-alpha
class CORE_EXPORT CSSAlphaColorValue : public CSSValue {
 public:
  CSSAlphaColorValue(const CSSValue* origin_color, const CSSValue* alpha)
      : CSSValue(kAlphaColorClass),
        origin_color_(origin_color),
        alpha_(alpha) {}

  String CustomCSSText() const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

  bool Equals(const CSSAlphaColorValue& other) const;

  const CSSValue& OriginColor() const { return *origin_color_; }

  // Alpha will be nullptr if it was not specified.
  const CSSValue* Alpha() const { return alpha_.Get(); }

  bool HasRandomFunctions() const {
    return origin_color_->HasRandomFunctions() ||
           (alpha_ && alpha_->HasRandomFunctions());
  }

 private:
  Member<const CSSValue> origin_color_;
  Member<const CSSValue> alpha_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSAlphaColorValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsAlphaColorValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ALPHA_COLOR_VALUE_H_

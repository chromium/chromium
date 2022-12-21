// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_quad_value.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

String CSSQuadValue::CustomCSSText() const {
  String top = top_->CssText();
  String right = right_->CssText();
  String bottom = bottom_->CssText();
  String left = left_->CssText();

  if (serialization_type_ == TypeForSerialization::kSerializeAsRect) {
    return "rect(" + top + ", " + right + ", " + bottom + ", " + left + ')';
  }

  StringBuilder result;
  // reserve space for the four strings, plus three space separator characters.
  result.ReserveCapacity(top.length() + right.length() + bottom.length() +
                         left.length() + 3);
  result.Append(top);
  if (right != top || bottom != top || left != top) {
    result.Append(' ');
    result.Append(right);
    if (bottom != top || right != left) {
      result.Append(' ');
      result.Append(bottom);
      if (left != right) {
        result.Append(' ');
        result.Append(left);
      }
    }
  }
  return result.ReleaseString();
}

void CSSQuadValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(top_);
  visitor->Trace(right_);
  visitor->Trace(bottom_);
  visitor->Trace(left_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink

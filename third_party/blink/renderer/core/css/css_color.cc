// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSColor* CSSColor::Create(const Color& color) {
  return CssValuePool().GetOrCreateColor(color);
}

String CSSColor::SerializeAsCSSComponentValue(Color color) {
  StringBuilder result;
  result.ReserveCapacity(32);
  bool color_has_alpha = color.HasAlpha();
  if (color_has_alpha)
    result.Append("rgba(");
  else
    result.Append("rgb(");

  result.AppendNumber(static_cast<uint16_t>(color.Red()));
  result.Append(", ");

  result.AppendNumber(static_cast<uint16_t>(color.Green()));
  result.Append(", ");

  result.AppendNumber(static_cast<uint16_t>(color.Blue()));
  if (color_has_alpha) {
    result.Append(", ");
    // See <alphavalue> section in
    // https://drafts.csswg.org/cssom/#serializing-css-values
    int alphavalue = color.Alpha();
    float rounded = round(alphavalue * 100 / 255.0f) / 100;
    if (round(rounded * 255) == alphavalue) {
      result.AppendNumber(rounded, 2);
    } else {
      rounded = round(alphavalue * 1000 / 255.0f) / 1000;
      result.AppendNumber(rounded, 3);
    }
  }

  result.Append(')');
  return result.ReleaseString();
}

}  // namespace cssvalue
}  // namespace blink

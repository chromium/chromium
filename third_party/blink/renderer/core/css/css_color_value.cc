// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_color_value.h"

#include "third_party/blink/renderer/core/css/css_value_pool.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

CSSColorValue* CSSColorValue::Create(RGBA32 color) {
  // These are the empty and deleted values of the hash table.
  if (color == Color::kTransparent)
    return CssValuePool().TransparentColor();
  if (color == Color::kWhite)
    return CssValuePool().WhiteColor();
  // Just because it is common.
  if (color == Color::kBlack)
    return CssValuePool().BlackColor();

  CSSValuePool::ColorValueCache::AddResult entry =
      CssValuePool().GetColorCacheEntry(color);
  if (entry.is_new_entry)
    entry.stored_value->value = MakeGarbageCollected<CSSColorValue>(color);
  return entry.stored_value->value;
}

String CSSColorValue::SerializeAsCSSComponentValue(Color color) {
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
  return result.ToString();
}

}  // namespace cssvalue
}  // namespace blink

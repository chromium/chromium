// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_dict_conversions.h"

#include "base/check_op.h"
#include "base/values.h"
#include "pdf/pdf_ink_text.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

namespace {

// Check if `color` is a valid color value within range.
void CheckColorIsWithinRange(int color) {
  CHECK_GE(color, 0);
  CHECK_LE(color, 255);
}

}  // namespace

SkColor GetColorFromDict(const base::DictValue& dict) {
  const base::DictValue& color = *dict.FindDict("color");
  int color_r = color.FindInt("r").value();
  int color_g = color.FindInt("g").value();
  int color_b = color.FindInt("b").value();

  CheckColorIsWithinRange(color_r);
  CheckColorIsWithinRange(color_g);
  CheckColorIsWithinRange(color_b);
  return SkColorSetRGB(color_r, color_g, color_b);
}

base::DictValue SkColorToDict(SkColor color) {
  return base::DictValue()
      .Set("r", static_cast<int>(SkColorGetR(color)))
      .Set("g", static_cast<int>(SkColorGetG(color)))
      .Set("b", static_cast<int>(SkColorGetB(color)));
}

gfx::RectF GetRectFromDict(const base::DictValue& rect_dict) {
  return gfx::RectF(rect_dict.FindDouble("locationX").value(),
                    rect_dict.FindDouble("locationY").value(),
                    rect_dict.FindDouble("width").value(),
                    rect_dict.FindDouble("height").value());
}

base::DictValue RectToDict(const gfx::RectF& rect) {
  return base::DictValue()
      .Set("height", static_cast<double>(rect.height()))
      .Set("locationX", static_cast<double>(rect.x()))
      .Set("locationY", static_cast<double>(rect.y()))
      .Set("width", static_cast<double>(rect.width()));
}

TextStyles GetStylesFromDict(const base::DictValue& styles_dict) {
  return TextStyles{
      .is_bold = styles_dict.FindBool("bold").value(),
      .is_italic = styles_dict.FindBool("italic").value(),
      .is_strikethrough = styles_dict.FindBool("strikethrough").value(),
      .is_underline = styles_dict.FindBool("underline").value(),
  };
}

base::DictValue StylesToDict(const TextStyles& styles) {
  return base::DictValue()
      .Set("bold", styles.is_bold)
      .Set("italic", styles.is_italic)
      .Set("strikethrough", styles.is_strikethrough)
      .Set("underline", styles.is_underline);
}

}  // namespace chrome_pdf

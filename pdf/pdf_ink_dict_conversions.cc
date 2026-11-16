// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_dict_conversions.h"

#include "base/check_op.h"
#include "base/values.h"
#include "third_party/skia/include/core/SkColor.h"

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

}  // namespace chrome_pdf

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_conversions.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/ink/src/ink/strokes/input/stroke_input.h"
#include "ui/gfx/geometry/point_f.h"

namespace chrome_pdf {

TEST(PdfInkConversionsTest, CreateInkStrokeInputWithPropertiesWithPressure) {
  blink::WebPointerProperties properties(/*id_param=*/0);
  properties.force = 0.5f;
  ink::StrokeInput input = CreateInkStrokeInputWithProperties(
      ink::StrokeInput::ToolType::kStylus, gfx::PointF(1.0f, 2.0f),
      base::Seconds(123), &properties);
  EXPECT_EQ(input.tool_type, ink::StrokeInput::ToolType::kStylus);
  EXPECT_EQ(input.position.x, 1.0f);
  EXPECT_EQ(input.position.y, 2.0f);
  EXPECT_EQ(input.elapsed_time.ToSeconds(), 123);
  EXPECT_EQ(input.pressure, 0.5f);
}

TEST(PdfInkConversionsTest, CreateInkStrokeInputWithPropertiesNoPressure) {
  ink::StrokeInput input = CreateInkStrokeInputWithProperties(
      ink::StrokeInput::ToolType::kStylus, gfx::PointF(1.0f, 2.0f),
      base::Seconds(123), /*properties=*/nullptr);
  EXPECT_EQ(input.tool_type, ink::StrokeInput::ToolType::kStylus);
  EXPECT_EQ(input.position.x, 1.0f);
  EXPECT_EQ(input.position.y, 2.0f);
  EXPECT_EQ(input.elapsed_time.ToSeconds(), 123);
  EXPECT_EQ(input.pressure, ink::StrokeInput::kNoPressure);
}

TEST(PdfInkConversionsTest,
     CreateInkStrokeInputWithPropertiesDefaultToNoPressure) {
  // Defaults to no pressure.
  const blink::WebPointerProperties properties(/*id_param=*/0);
  ink::StrokeInput input = CreateInkStrokeInputWithProperties(
      ink::StrokeInput::ToolType::kStylus, gfx::PointF(1.0f, 2.0f),
      base::Seconds(123), &properties);
  EXPECT_EQ(input.tool_type, ink::StrokeInput::ToolType::kStylus);
  EXPECT_EQ(input.position.x, 1.0f);
  EXPECT_EQ(input.position.y, 2.0f);
  EXPECT_EQ(input.elapsed_time.ToSeconds(), 123);
  EXPECT_EQ(input.pressure, ink::StrokeInput::kNoPressure);
}

}  // namespace chrome_pdf

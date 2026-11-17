// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_dict_conversions.h"

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "pdf/pdf_ink_text.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect_f.h"

namespace chrome_pdf {

TEST(PdfInkDictConversionsTest, GetColorFromDict) {
  EXPECT_EQ(SK_ColorBLACK, GetColorFromDict(base::test::ParseJsonDict(
                               R"({"color": {"r": 0, "g": 0, "b": 0}})")));
  EXPECT_EQ(SK_ColorWHITE,
            GetColorFromDict(base::test::ParseJsonDict(
                R"({"color": {"r": 255, "g": 255, "b": 255}})")));
  EXPECT_EQ(SkColorSetRGB(18, 164, 175),
            GetColorFromDict(base::test::ParseJsonDict(
                R"({"color": {"r": 18, "g": 164, "b": 175}})")));
}

TEST(PdfInkDictConversionsTest, SkColorToDict) {
  EXPECT_THAT(SkColorToDict(SK_ColorBLACK),
              base::test::IsJson(R"({"r": 0, "g": 0, "b": 0})"));
  EXPECT_THAT(SkColorToDict(SK_ColorWHITE),
              base::test::IsJson(R"({"r": 255, "g": 255, "b": 255})"));
  EXPECT_THAT(SkColorToDict(SkColorSetRGB(18, 164, 175)),
              base::test::IsJson(R"({"r": 18, "g": 164, "b": 175})"));
  EXPECT_THAT(SkColorToDict(SkColorSetARGB(128, 10, 20, 30)),
              base::test::IsJson(R"({"r": 10, "g": 20, "b": 30})"));
}

TEST(PdfInkDictConversionsTest, GetRectFromDict) {
  EXPECT_EQ(gfx::RectF(10.0f, 20.0f, 100.0f, 50.0f),
            GetRectFromDict(base::test::ParseJsonDict(R"({
              "locationX": 10.0,
              "locationY": 20.0,
              "width": 100.0,
              "height": 50.0,
            })")));
}

TEST(PdfInkDictConversionsTest, RectToDict) {
  EXPECT_THAT(RectToDict(gfx::RectF(10.0f, 20.0f, 100.0f, 50.0f)),
              base::test::IsJson(R"({
                "locationX": 10.0,
                "locationY": 20.0,
                "width": 100.0,
                "height": 50.0,
              })"));
}

TEST(PdfInkDictConversionsTest, GetStylesFromDict) {
  EXPECT_EQ((TextStyles{
                .is_bold = true,
                .is_italic = false,
                .is_strikethrough = true,
                .is_underline = false,
            }),
            GetStylesFromDict(base::test::ParseJsonDict(R"({
              "bold": true,
              "italic": false,
              "strikethrough": true,
              "underline": false,
            })")));
  EXPECT_EQ((TextStyles{
                .is_bold = false,
                .is_italic = true,
                .is_strikethrough = false,
                .is_underline = true,
            }),
            GetStylesFromDict(base::test::ParseJsonDict(R"({
              "bold": false,
              "italic": true,
              "strikethrough": false,
              "underline": true,
            })")));
}

TEST(PdfInkDictConversionsTest, StylesToDict) {
  EXPECT_THAT(StylesToDict(TextStyles{
                  .is_bold = true,
                  .is_italic = false,
                  .is_strikethrough = true,
                  .is_underline = false,
              }),
              base::test::IsJson(R"({
                "bold": true,
                "italic": false,
                "strikethrough": true,
                "underline": false,
              })"));
  EXPECT_THAT(StylesToDict(TextStyles{
                  .is_bold = false,
                  .is_italic = true,
                  .is_strikethrough = false,
                  .is_underline = true,
              }),
              base::test::IsJson(R"({
                "bold": false,
                "italic": true,
                "strikethrough": false,
                "underline": true,
              })"));
}

}  // namespace chrome_pdf

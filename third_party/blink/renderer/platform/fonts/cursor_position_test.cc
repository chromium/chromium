// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/testing/font_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using blink::test::CreateTestFont;

namespace blink {

class CursorPositionTest : public ::testing::Test {
 public:
  enum FontName {
    ahem,
    amiri,
    megalopolis,
    roboto,
  };

  float GetWidth(FontName font_name,
                 const String& text,
                 bool ltr,
                 int start = 0,
                 int end = -1) {
    FontDescription::VariantLigatures ligatures(
        FontDescription::kEnabledLigaturesState);
    Font font = CreateTestFont(
        "TestFont",
        test::PlatformTestDataPath(font_path.find(font_name)->value), 100,
        &ligatures);
    TextRun text_run(
        text, /* xpos */ 0, /* expansion */ 0,
        TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
        ltr ? TextDirection::kLtr : TextDirection::kRtl, false);

    if (end == -1)
      end = text_run.length();
    DCHECK_GE(start, 0);
    DCHECK_LE(start, static_cast<int>(text_run.length()));
    DCHECK_GE(end, -1);
    DCHECK_LE(end, static_cast<int>(text_run.length()));
    FloatRect rect =
        font.SelectionRectForText(text_run, FloatPoint(), 12, start, end);
    return rect.Width();
  }

  int GetCharacter(FontName font_name,
                   const String& text,
                   bool ltr,
                   float position,
                   bool partial) {
    FontDescription::VariantLigatures ligatures(
        FontDescription::kEnabledLigaturesState);
    Font font = CreateTestFont(
        "TestFont",
        test::PlatformTestDataPath(font_path.find(font_name)->value), 100,
        &ligatures);
    TextRun text_run(
        text, /* xpos */ 0, /* expansion */ 0,
        TextRun::kAllowTrailingExpansion | TextRun::kForbidLeadingExpansion,
        ltr ? TextDirection::kLtr : TextDirection::kRtl, false);

    return font.OffsetForPosition(
        text_run, position, partial ? IncludePartialGlyphs : OnlyFullGlyphs,
        BreakGlyphs);
  }

 private:
  HashMap<FontName, String, WTF::IntHash<FontName>> font_path = {
      {ahem, "Ahem.woff"},
      {amiri, "third_party/Amiri/amiri_arabic.woff2"},
      {megalopolis, "third_party/MEgalopolis/MEgalopolisExtra.woff"},
      {roboto, "third_party/Roboto/roboto-regular.woff2"},
  };
};

TEST_F(CursorPositionTest, LTRMouse) {
  EXPECT_EQ(GetCharacter(ahem, "X", true, 0, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 0, true), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 10, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 10, true), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 60, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 60, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 100, false), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", true, 100, true), 1);

  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 10, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 10, true), 0);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 60, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 60, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 100, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 100, false), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 125, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 125, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 151, false), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 151, true), 2);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 175, false), 1);
  EXPECT_EQ(GetCharacter(ahem, "XXX", true, 175, true), 2);
}

TEST_F(CursorPositionTest, LTRLigatureMouse) {
  const float kFUWidth = GetWidth(megalopolis, "FU", true);
  const float kRAWidth = GetWidth(megalopolis, "RA", true);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 4 - 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 4 - 1, true), 0);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 4 + 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 4 + 1, true), 1);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 2 - 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 2 - 1, true), 1);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 2 + 1, false),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth / 2 + 1, true), 1);

  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth * 3 / 4 - 1, false), 1);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth * 3 / 4 - 1, true),
            1);
  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth * 3 / 4 + 1, false), 1);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth * 3 / 4 + 1, true),
            2);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth - 1, false), 1);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth - 1, true), 2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + 1, false), 2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + 1, true), 2);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 4 - 1,
                         false),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 4 - 1,
                         true),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 4 + 1,
                         false),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 4 + 1,
                         true),
            3);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 2 - 1,
                         false),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 2 - 1,
                         true),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 2 + 1,
                         false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth / 2 + 1,
                         true),
            3);

  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true,
                         kFUWidth + kRAWidth * 3 / 4 - 1, false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true,
                         kFUWidth + kRAWidth * 3 / 4 - 1, true),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true,
                         kFUWidth + kRAWidth * 3 / 4 + 1, false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "FURA", true,
                         kFUWidth + kRAWidth * 3 / 4 + 1, true),
            4);

  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth - 1, false),
      3);
  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth - 1, true),
      4);
  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth + 1, false),
      4);
  EXPECT_EQ(
      GetCharacter(megalopolis, "FURA", true, kFUWidth + kRAWidth + 1, true),
      4);
}

TEST_F(CursorPositionTest, RTLMouse) {
  // The widths below are from the final shaped version, not from the single
  // characters. They were extracted with "hb-shape --font-size=100"

  EXPECT_EQ(GetCharacter(ahem, "X", false, 0, false), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 0, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 10, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 10, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 49, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 49, true), 1);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 51, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 51, true), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 60, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 60, true), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 100, false), 0);
  EXPECT_EQ(GetCharacter(ahem, "X", false, 100, true), 0);

  const float kAloneTaWidth = GetWidth(amiri, u"ت", false);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, 0, false), 1);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, 0, true), 1);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, kAloneTaWidth / 4, false), 0);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, kAloneTaWidth / 4, true), 1);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, kAloneTaWidth * 2 / 3, false), 0);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, kAloneTaWidth * 2 / 3, true), 0);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, 2 * kAloneTaWidth, false), 0);
  EXPECT_EQ(GetCharacter(amiri, u"ت", false, 2 * kAloneTaWidth, true), 0);

  const float kAboveTaWidth = 10;
  const float kAboveKhaWidth = 55;
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, 0, false), 2);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, 0, true), 2);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth / 4, false), 1);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth / 4, true), 2);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth * 2 / 3, false), 1);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth * 2 / 3, true), 1);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth + 1, false), 0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false, kAboveTaWidth + 1, true), 1);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth / 4, false),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth / 4, true),
            1);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth * 2 / 3, false),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth * 2 / 3, true),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth + 1, false),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         kAboveTaWidth + kAboveKhaWidth + 1, true),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         2 * (kAboveTaWidth + kAboveKhaWidth), false),
            0);
  EXPECT_EQ(GetCharacter(amiri, u"تخ", false,
                         2 * (kAboveTaWidth + kAboveKhaWidth), true),
            0);
}

TEST_F(CursorPositionTest, RTLLigatureMouse) {
  const float kFUWidth = GetWidth(megalopolis, "FU", true);
  const float kRAWidth = GetWidth(megalopolis, "RA", true);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 4 - 1, false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 4 - 1, true),
            4);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 4 + 1, false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 4 + 1, true),
            3);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 2 - 1, false),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 2 - 1, true),
            3);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 2 + 1, false),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth / 2 + 1, true),
            3);

  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth * 3 / 4 - 1, false), 2);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth * 3 / 4 - 1, true), 3);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth * 3 / 4 + 1, false), 2);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth * 3 / 4 + 1, true), 2);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth - 1, false), 2);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth - 1, true), 2);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth + 1, false), 1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false, kFUWidth + 1, true), 2);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 4 - 1, false),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 4 - 1, true),
            2);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 4 + 1, false),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 4 + 1, true),
            1);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 2 - 1, false),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 2 - 1, true),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 2 + 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth / 2 + 1, true),
            1);

  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth * 3 / 4 - 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth * 3 / 4 - 1, true),
            1);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth * 3 / 4 + 1, false),
            0);
  EXPECT_EQ(GetCharacter(megalopolis, "ARUF", false,
                         kFUWidth + kRAWidth * 3 / 4 + 1, true),
            0);

  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth + kRAWidth - 1, false),
      0);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth + kRAWidth - 1, true),
      0);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth + kRAWidth + 1, false),
      0);
  EXPECT_EQ(
      GetCharacter(megalopolis, "ARUF", false, kFUWidth + kRAWidth + 1, true),
      0);
}

TEST_F(CursorPositionTest, LTRText) {
  EXPECT_EQ(GetWidth(ahem, "X", true, 0, 1), 100);

  EXPECT_EQ(GetWidth(ahem, "XXX", true, 0, 1), 100);
  EXPECT_EQ(GetWidth(ahem, "XXX", true, 0, 2), 200);
  EXPECT_EQ(GetWidth(ahem, "XXX", true, 0, 3), 300);
  EXPECT_EQ(GetWidth(ahem, "XXX", true, 1, 2), 100);
  EXPECT_EQ(GetWidth(ahem, "XXX", true, 1, 3), 200);
  EXPECT_EQ(GetWidth(ahem, "XXX", true, 2, 3), 100);
}

TEST_F(CursorPositionTest, LTRLigature) {
  const float kFUWidth = GetWidth(megalopolis, "FU", true);
  const float kRAWidth = GetWidth(megalopolis, "RA", true);

  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 0, 1), kFUWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 0, 2), kFUWidth, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 0, 3),
              kFUWidth + kRAWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 0, 4), kFUWidth + kRAWidth,
              1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 1, 2), kFUWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 1, 3),
              kFUWidth / 2 + kRAWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 1, 4),
              kFUWidth / 2 + kRAWidth, 1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 2, 3), kRAWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 2, 4), kRAWidth, 1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "FURA", true, 3, 4), kRAWidth / 2, 1.0);

  const float kFFIWidth = GetWidth(roboto, "ffi", true);
  const float kFFWidth = GetWidth(roboto, "ff", true);
  const float kIWidth = GetWidth(roboto, u"î", true);

  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 0, 1), kFFIWidth / 3.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 0, 2), kFFIWidth * 2.0 / 3.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 0, 3), kFFIWidth, 1.0);
  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 1, 2), kFFIWidth / 3.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 1, 3), kFFIWidth * 2.0 / 3.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, "ffi", true, 2, 3), kFFIWidth / 3.0, 1.0);

  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 0, 1), kFFWidth / 2.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 0, 2), kFFWidth, 1.0);
  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 0, 3), kFFWidth + kIWidth, 1.0);
  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 1, 2), kFFWidth / 2.0, 1.0);
  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 1, 3), kFFWidth / 2.0 + kIWidth,
              1.0);
  EXPECT_NEAR(GetWidth(roboto, u"ffî", true, 2, 3), kIWidth, 1.0);
}

TEST_F(CursorPositionTest, RTLText) {
  // The widths below are from the final shaped version, not from the single
  // characters. They were extracted with "hb-shape --font-size=100"

  EXPECT_EQ(GetWidth(amiri, u"ت", false, 0, 1), 93);

  const float kAboveKhaWidth = 55;
  const float kAboveTaWidth = 10;
  EXPECT_NEAR(GetWidth(amiri, u"تخ", false, 0, 1), kAboveKhaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"تخ", false, 0, 2),
              kAboveKhaWidth + kAboveTaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"تخ", false, 1, 2), kAboveTaWidth, 1.0);

  const float kTaWidth = 75;
  const float kKhaWidth = 7;
  const float kLamWidth = 56;
  const float kAlifWidth = 22;
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 0, 1), kAlifWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 0, 2), kAlifWidth + kLamWidth,
              1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 0, 3),
              kAlifWidth + kLamWidth + kKhaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 0, 4),
              kAlifWidth + kLamWidth + kKhaWidth + kTaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 1, 2), kLamWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 1, 3), kLamWidth + kKhaWidth,
              1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 1, 4),
              kLamWidth + kKhaWidth + kTaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 2, 3), kKhaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 2, 4), kKhaWidth + kTaWidth, 1.0);
  EXPECT_NEAR(GetWidth(amiri, u"الخط", false, 3, 4), kTaWidth, 1.0);

  const float kMeemWidth = GetWidth(amiri, u"م", false);
  EXPECT_EQ(GetWidth(amiri, u"مَ", false, 0, 1), kMeemWidth);
  EXPECT_EQ(GetWidth(amiri, u"مَ", false, 0, 2), kMeemWidth);
  EXPECT_EQ(GetWidth(amiri, u"مَ", false, 1, 2), kMeemWidth);
}

TEST_F(CursorPositionTest, RTLLigature) {
  const float kFUWidth = GetWidth(megalopolis, "FU", true);
  const float kRAWidth = GetWidth(megalopolis, "RA", true);

  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 0, 1), kRAWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 0, 2), kRAWidth, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 0, 3),
              kRAWidth + kFUWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 0, 4), kRAWidth + kFUWidth,
              1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 1, 2), kRAWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 1, 3),
              kRAWidth / 2 + kFUWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 1, 4),
              kRAWidth / 2 + kFUWidth, 1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 2, 3), kFUWidth / 2, 1.0);
  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 2, 4), kFUWidth, 1.0);

  EXPECT_NEAR(GetWidth(megalopolis, "ARUF", false, 3, 4), kFUWidth / 2, 1.0);
}

}  // namespace blink

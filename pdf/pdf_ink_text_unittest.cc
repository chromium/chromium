// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_text.h"

#include <vector>

#include "pdf/test/pdf_ink_test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::SizeIs;

namespace chrome_pdf {
namespace {

pdf::mojom::InkTextRunPtr MakeTextRunWithText(
    gfx::RectF location,
    std::vector<std::vector<float>> typeface_run_total_advance,
    std::vector<std::vector<gfx::Vector2dF>> typeface_run_glyph_offset,
    std::u16string text,
    std::vector<uint32_t> character_index) {
  if (!typeface_run_glyph_offset.empty()) {
    CHECK_EQ(typeface_run_total_advance.size(),
             typeface_run_glyph_offset.size());
  }
  if (!character_index.empty()) {
    size_t total = 0;
    for (const std::vector<float>& vec : typeface_run_total_advance) {
      total += vec.size();
    }
    CHECK_EQ(character_index.size(), total);
  }
  auto text_run = pdf::mojom::InkTextRun::New();
  text_run->location = location;
  text_run->text = text;

  size_t glyph_idx = 0;
  for (size_t i = 0; i < typeface_run_total_advance.size(); ++i) {
    const std::vector<float>& glyph_total_advance =
        typeface_run_total_advance[i];
    const std::vector<gfx::Vector2dF>* glyph_offset = nullptr;
    if (!typeface_run_glyph_offset.empty()) {
      glyph_offset = &typeface_run_glyph_offset[i];
      CHECK_EQ(glyph_offset->size(), glyph_total_advance.size());
    }
    auto typeface_run = pdf::mojom::InkTypefaceRun::New();
    typeface_run->is_horizontal = true;
    for (size_t j = 0; j < glyph_total_advance.size(); ++j) {
      auto glyph = pdf::mojom::InkGlyphInfo::New();
      // Set the glyph IDs to 101, 102, 103... for the first typeface_run;
      // 201, 202, 203... for the second typeface_run; and so on.
      glyph->glyph = (j + 1) + (100 * i);
      glyph->total_advance = glyph_total_advance[j];
      if (glyph_offset) {
        glyph->offset = (*glyph_offset)[j];
      }
      if (!character_index.empty()) {
        glyph->character_index = character_index[glyph_idx++];
      }
      typeface_run->glyphs.push_back(std::move(glyph));
    }
    text_run->typeface_runs.push_back(std::move(typeface_run));
  }
  return text_run;
}

pdf::mojom::InkTextRunPtr MakeTextRun(
    gfx::RectF location,
    std::vector<std::vector<float>> typeface_run_total_advance,
    std::vector<std::vector<gfx::Vector2dF>> typeface_run_glyph_offset) {
  return MakeTextRunWithText(location, typeface_run_total_advance,
                             typeface_run_glyph_offset, u"", {});
}

}  // namespace

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, NoOffset) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 100.0f, 70.0f, 20.0f),
                                  /*typeface_run_total_advance=*/
                                  {
                                      {0.0f, 10.0f, 20.0f, 30.0f, 40.0f},
                                      {50.0f, 60.0f},
                                  },
                                  {}));
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
                                  /*typeface_run_total_advance=*/
                                  {
                                      {0.0f, 10.0f, 20.0f},
                                      {30.0f, 40.0f},
                                  },
                                  {}));
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
                                  /*typeface_run_total_advance=*/
                                  {
                                      {0.0f, 10.0f, 20.0f},
                                  },
                                  {}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(3));

  EXPECT_EQ(ink_lines[0].location, gfx::RectF(10.0f, 10.0f, 7.0f, 2.0f));
  ASSERT_THAT(ink_lines[0].text_info, SizeIs(2));
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoEq(
          FontId(0),
          /*glyphs=*/std::vector<uint32_t>{1, 2, 3, 4, 5},
          /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
          /*location=*/gfx::RectF(10.0f, 10.0f, 5.0f, 2.0f),
          /*is_horizontal=*/true));
  EXPECT_THAT(ink_lines[0].text_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.0f},
                            /*location=*/gfx::RectF(15.0f, 10.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_EQ(ink_lines[1].location, gfx::RectF(10.0f, 20.0f, 5.0f, 2.0f));
  ASSERT_THAT(ink_lines[1].text_info, SizeIs(2));
  EXPECT_THAT(
      ink_lines[1].text_info[0],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 3.0f, 2.0f),
                    /*is_horizontal=*/true));
  EXPECT_THAT(ink_lines[1].text_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.0f},
                            /*location=*/gfx::RectF(13.0f, 20.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_EQ(ink_lines[2].location, gfx::RectF(10.0f, 20.0f, 4.0f, 2.0f));
  ASSERT_THAT(ink_lines[2].text_info, SizeIs(1));
  EXPECT_THAT(
      ink_lines[2].text_info[0],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 4.0f, 2.0f),
                    /*is_horizontal=*/true));
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, HorizontalOffset) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(
      MakeTextRun(gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
                  /*typeface_run_total_advance=*/
                  {
                      {0.0f, 10.0f, 20.0f},
                      {30.0f, 40.0f},
                  },
                  {{gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
                    gfx::Vector2dF(-4.0f, 0.0f)},
                   {gfx::Vector2dF(5.0f, 0.0f), gfx::Vector2dF(-2.0f, 0.0f)}}));
  text_runs.push_back(
      MakeTextRun(gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
                  /*typeface_run_total_advance=*/
                  {
                      {0.0f, 10.0f, 20.0f},
                  },
                  {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
                    gfx::Vector2dF(3.0f, 0.0f)}}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(2));

  ASSERT_THAT(ink_lines[0].text_info, SizeIs(2));
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.5f, 1.6f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 3.5f, 2.0f),
                    /*is_horizontal=*/true));

  EXPECT_THAT(ink_lines[0].text_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 1.5f, 2.0f),
                            /*is_horizontal=*/true));

  ASSERT_THAT(ink_lines[1].text_info, SizeIs(1));
  EXPECT_THAT(
      ink_lines[1].text_info[0],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.9f, 2.7f},
                    /*location=*/gfx::RectF(9.6f, 20.0f, 4.4f, 2.0f),
                    /*is_horizontal=*/true));
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, 2DOffset) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(
      MakeTextRun(gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
                  /*typeface_run_total_advance=*/
                  {
                      {0.0f, 10.0f, 20.0f},
                      {30.0f, 40.0f},
                  },
                  {{gfx::Vector2dF(0.0f, 5.0f), gfx::Vector2dF(4.0f, 0.0f),
                    gfx::Vector2dF(-1.0f, 0.0f)},
                   {gfx::Vector2dF(5.0f, 0.0f), gfx::Vector2dF(-2.0f, 0.0f)}}));
  text_runs.push_back(
      MakeTextRun(gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
                  /*typeface_run_total_advance=*/
                  {
                      {0.0f, 10.0f, 20.0f},
                  },
                  {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
                    gfx::Vector2dF(3.0f, 5.0f)}}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(2));

  ASSERT_THAT(ink_lines[0].text_info, SizeIs(3));
  EXPECT_THAT(ink_lines[0].text_info[0],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.5f, 1.4f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_lines[0].text_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2, 3},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.5f},
                            /*location=*/gfx::RectF(11.4f, 20.0f, 2.1f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_lines[0].text_info[2],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 1.5f, 2.0f),
                            /*is_horizontal=*/true));

  ASSERT_THAT(ink_lines[1].text_info, SizeIs(2));
  EXPECT_THAT(ink_lines[1].text_info[0],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1, 2},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.9f},
                            /*location=*/gfx::RectF(9.6f, 20.0f, 2.7f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_lines[1].text_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(12.3f, 20.5f, 1.7f, 2.0f),
                            /*is_horizontal=*/true));
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, SplitText) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
      /*typeface_run_total_advance=*/
      {
          {0.0f, 10.0f, 20.0f},
          {30.0f, 40.0f},
      },
      {{gfx::Vector2dF(0.0f, 5.0f), gfx::Vector2dF(4.0f, 0.0f),
        gfx::Vector2dF(-1.0f, 0.0f)},
       {gfx::Vector2dF(5.0f, 0.0f), gfx::Vector2dF(-2.0f, 0.0f)}},
      u"12345", {0, 1, 1, 2, 4}));
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
      /*typeface_run_total_advance=*/
      {
          {0.0f, 10.0f, 20.0f},
      },
      {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
        gfx::Vector2dF(3.0f, 5.0f)}},
      u"678", {0, 1, 2}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(2));

  ASSERT_THAT(ink_lines[0].text_info, SizeIs(3));
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.5f, 1.4f, 2.0f),
                            /*is_horizontal=*/true, u"1",
                            /*join_prev_actualtext=*/false));

  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2, 3},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.5f},
                            /*location=*/gfx::RectF(11.4f, 20.0f, 2.1f, 2.0f),
                            /*is_horizontal=*/true, u"2",
                            /*join_prev_actualtext=*/false));

  EXPECT_THAT(
      ink_lines[0].text_info[2],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 1.5f, 2.0f),
                            /*is_horizontal=*/true, u"345",
                            /*join_prev_actualtext=*/false));

  ASSERT_THAT(ink_lines[1].text_info, SizeIs(2));
  EXPECT_THAT(
      ink_lines[1].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1, 2},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.9f},
                            /*location=*/gfx::RectF(9.6f, 20.0f, 2.7f, 2.0f),
                            /*is_horizontal=*/true, u"67",
                            /*join_prev_actualtext=*/false));

  EXPECT_THAT(
      ink_lines[1].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(12.3f, 20.5f, 1.7f, 2.0f),
                            /*is_horizontal=*/true, u"8",
                            /*join_prev_actualtext=*/false));
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, SplitText2D) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
      /*typeface_run_total_advance=*/
      {
          {0.0f, 10.0f, 20.0f},
          {30.0f, 40.0f},
      },
      {{gfx::Vector2dF(0.0f, 5.0f), gfx::Vector2dF(4.0f, 0.0f),
        gfx::Vector2dF(-1.0f, 5.0f)},
       {gfx::Vector2dF(5.0f, 0.0f), gfx::Vector2dF(-2.0f, 0.0f)}},
      u"12345", {0, 1, 1, 2, 4}));
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
      /*typeface_run_total_advance=*/
      {
          {0.0f, 10.0f, 20.0f},
      },
      {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
        gfx::Vector2dF(3.0f, 5.0f)}},
      u"678", {0, 1, 2}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(2));

  ASSERT_THAT(ink_lines[0].text_info, SizeIs(4));
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.5f, 1.4f, 2.0f),
                            /*is_horizontal=*/true, u"1",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): this text_info incorrectly has u"" for
  // /ActualText leading to /ActualText<FEFF> in the PDF.
  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(11.4f, 20.0f, 0.5f, 2.0f),
                            /*is_horizontal=*/true, u"",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): this text_info and the previous one should be
  // joined together into a single /ActualText span with the text u"2"
  EXPECT_THAT(
      ink_lines[0].text_info[2],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/
                            std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(11.9f, 20.5f, 1.6f, 2.0f),
                            /*is_horizontal=*/true, u"2",
                            /*join_prev_actualtext=*/false));

  EXPECT_THAT(
      ink_lines[0].text_info[3],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 1.5f, 2.0f),
                            /*is_horizontal=*/true, u"345",
                            /*join_prev_actualtext=*/false));

  ASSERT_THAT(ink_lines[1].text_info, SizeIs(2));
  EXPECT_THAT(
      ink_lines[1].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1, 2},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.9f},
                            /*location=*/gfx::RectF(9.6f, 20.0f, 2.7f, 2.0f),
                            /*is_horizontal=*/true, u"67",
                            /*join_prev_actualtext=*/false));

  EXPECT_THAT(
      ink_lines[1].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(12.3f, 20.5f, 1.7f, 2.0f),
                            /*is_horizontal=*/true, u"8",
                            /*join_prev_actualtext=*/false));
}

// In this test, glyph 1 is baseline text (character "1") with y-offset 0.0.
// Glyphs 2 and 3 belong to the same cluster (character "2"), where glyph 2 has
// y-offset 0.0 and glyph 3 has y-offset 5.0.
TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest,
     SplitText2DClusterPrecededByBaseline) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 30.0f, 20.0f),
      /*typeface_run_total_advance=*/{{0.0f, 10.0f, 20.0f}},
      {{gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(4.0f, 0.0f),
        gfx::Vector2dF(-1.0f, 5.0f)}},
      u"12", {0, 1, 1}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(1));
  ASSERT_THAT(ink_lines[0].text_info, SizeIs(2));

  // TODO(crbug.com/525093248): Glyph 1 should be separated into its own
  // InkTextInfo with text "1", and glyph 2 should have text "2".
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1, 2},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.4f},
                            /*location=*/gfx::RectF(10.0f, 20.0f, 1.9f, 2.0f),
                            /*is_horizontal=*/true, u"1",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Glyph 3 should have join_prev_actualtext = true
  // and be joined with glyph 2 under /ActualText "2".
  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(11.9f, 20.5f, 1.1f, 2.0f),
                            /*is_horizontal=*/true, u"2",
                            /*join_prev_actualtext=*/false));
}

// In this test, glyphs 1 and 2 belong to the same cluster (character "1"),
// where glyph 1 is a mark with y-offset 5.0 and glyph 2 is a base with
// y-offset 0.0. Glyph 3 is subsequent baseline text (character "2") with
// y-offset 0.0.
TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest,
     SplitText2DClusterFollowedByBaseline) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 30.0f, 20.0f),
      /*typeface_run_total_advance=*/{{0.0f, 10.0f, 20.0f}},
      {{gfx::Vector2dF(0.0f, 5.0f), gfx::Vector2dF(4.0f, 0.0f),
        gfx::Vector2dF(0.0f, 0.0f)}},
      u"12", {0, 0, 1}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(1));
  ASSERT_THAT(ink_lines[0].text_info, SizeIs(2));

  // TODO(crbug.com/525093248): Slice 0 should have text "1" (or "" if joined
  // with glyph 2), not "12".
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.5f, 1.4f, 2.0f),
                            /*is_horizontal=*/true, u"",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Glyph 3 should be in its own InkTextInfo with
  // text "2" and join_prev_actualtext = false.
  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2, 3},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.6f},
                            /*location=*/gfx::RectF(11.4f, 20.0f, 1.6f, 2.0f),
                            /*is_horizontal=*/true, u"12",
                            /*join_prev_actualtext=*/false));
}

// In this test, adjacent characters both have diacritics at y-offset 5.0:
// Cluster 0: base glyph 1 (y=0.0), mark glyph 2 (y=5.0)
// Cluster 1: mark glyph 3 (y=5.0), base glyph 4 (y=0.0)
TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest,
     SplitText2DConsecutiveSplitClustersSharingOffset) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
      /*typeface_run_total_advance=*/{{0.0f, 10.0f, 20.0f, 30.0f}},
      {{gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(0.0f, 5.0f),
        gfx::Vector2dF(0.0f, 5.0f), gfx::Vector2dF(0.0f, 0.0f)}},
      u"12", {0, 0, 1, 1}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(1));
  ASSERT_THAT(ink_lines[0].text_info, SizeIs(3));

  // TODO(crbug.com/525093248): Slice 0 should have text "1" and be joined with
  // slice 1 (glyph 2) under /ActualText "1". Currently it has text u"".
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.0f, 1.0f, 2.0f),
                            /*is_horizontal=*/true, u"",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Glyphs 2 and 3 should not be merged across
  // clusters. Glyph 2 should be joined with glyph 1 under /ActualText "1",
  // and glyph 3 should be joined with glyph 4 under /ActualText "2".
  // Currently glyph 3 is absorbed into slice 1 under text "1".
  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2, 3},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.0f},
                            /*location=*/gfx::RectF(11.0f, 20.5f, 2.0f, 2.0f),
                            /*is_horizontal=*/true, u"1",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Glyph 4 should be joined with glyph 3 under
  // /ActualText "2". Currently glyph 3 was swallowed into slice 1, so slice 2
  // only contains glyph 4.
  EXPECT_THAT(
      ink_lines[0].text_info[2],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{4},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(13.0f, 20.0f, 1.0f, 2.0f),
                            /*is_horizontal=*/true, u"2",
                            /*join_prev_actualtext=*/false));
}

// In this test, a single cluster has multiple marks at distinct vertical
// positions: base glyph 1 (y=0.0), mark above glyph 2 (y=5.0), mark below
// glyph 3 (y=-5.0).
TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, SplitText2DThreeSlices) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRunWithText(
      gfx::RectF(100.0f, 200.0f, 30.0f, 20.0f),
      /*typeface_run_total_advance=*/{{0.0f, 10.0f, 20.0f}},
      {{gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(0.0f, 5.0f),
        gfx::Vector2dF(0.0f, -5.0f)}},
      u"1", {0, 0, 0}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_THAT(ink_lines, SizeIs(1));
  ASSERT_THAT(ink_lines[0].text_info, SizeIs(3));

  // TODO(crbug.com/525093248): Slice 0 should have text "1" and be joined with
  // slices 1 and 2 under a single /ActualText "1". Currently it has text u"".
  EXPECT_THAT(
      ink_lines[0].text_info[0],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.0f, 1.0f, 2.0f),
                            /*is_horizontal=*/true, u"",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Slice 1 should have join_prev_actualtext = true
  // and be joined with slice 0 under /ActualText "1".
  EXPECT_THAT(
      ink_lines[0].text_info[1],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(11.0f, 20.5f, 1.0f, 2.0f),
                            /*is_horizontal=*/true, u"",
                            /*join_prev_actualtext=*/false));

  // TODO(crbug.com/525093248): Slice 2 should have join_prev_actualtext = true
  // and be joined with slice 0 under /ActualText "1".
  EXPECT_THAT(
      ink_lines[0].text_info[2],
      InkTextInfoWithTextEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(12.0f, 19.5f, 1.0f, 2.0f),
                            /*is_horizontal=*/true, u"1",
                            /*join_prev_actualtext=*/false));
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, SyntheticBoldItalic) {
  auto glyph = pdf::mojom::InkGlyphInfo::New();
  glyph->glyph = 1;
  glyph->total_advance = 0.0f;

  auto typeface_run = pdf::mojom::InkTypefaceRun::New();
  typeface_run->typeface_id = 0;
  typeface_run->glyphs.push_back(std::move(glyph));
  typeface_run->is_horizontal = true;
  typeface_run->is_synthetic_bold = true;
  typeface_run->is_synthetic_italic = true;

  auto text_run = pdf::mojom::InkTextRun::New();
  text_run->typeface_runs.push_back(std::move(typeface_run));
  text_run->location = gfx::RectF(100.0f, 100.0f, 70.0f, 20.0f);
  text_run->text = u"X";

  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(std::move(text_run));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_EQ(ink_lines.size(), 1u);
  ASSERT_EQ(ink_lines[0].text_info.size(), 1u);
  EXPECT_TRUE(ink_lines[0].text_info[0].is_synthetic_bold);
  EXPECT_TRUE(ink_lines[0].text_info[0].is_synthetic_italic);
}

TEST(PdfInkTextBlinkTextInfoToPDFTextLinesTest, EmptyRunSkipped) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(
      MakeTextRun(gfx::RectF(100.0f, 100.0f, 70.0f, 20.0f),
                  /*typeface_run_total_advance=*/{{0.0f, 10.0f}}, {}));
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
                                  /*typeface_run_total_advance=*/{}, {}));

  std::vector<InkTextLine> ink_lines =
      InkTextLine::BlinkTextInfoToPDFTextLines(text_runs, 10.0f);
  ASSERT_EQ(ink_lines.size(), 1u);
  EXPECT_EQ(ink_lines[0].location, gfx::RectF(10.0f, 10.0f, 7.0f, 2.0f));
  ASSERT_EQ(ink_lines[0].text_info.size(), 1u);
}

}  // namespace chrome_pdf

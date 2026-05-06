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

pdf::mojom::InkTextRunPtr MakeTextRun(
    gfx::RectF location,
    std::vector<std::vector<float>> typeface_run_total_advance,
    std::vector<std::vector<gfx::Vector2dF>> typeface_run_glyph_offset) {
  if (!typeface_run_glyph_offset.empty()) {
    CHECK_EQ(typeface_run_total_advance.size(),
             typeface_run_glyph_offset.size());
  }
  auto text_run = pdf::mojom::InkTextRun::New();
  text_run->location = location;

  for (size_t i = 0; i < typeface_run_total_advance.size(); ++i) {
    const std::vector<float>& glyph_total_advance =
        typeface_run_total_advance[i];
    const std::vector<gfx::Vector2dF>* glyph_offset = nullptr;
    if (!typeface_run_glyph_offset.empty()) {
      glyph_offset = &typeface_run_glyph_offset[i];
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
      typeface_run->glyphs.push_back(std::move(glyph));
    }
    text_run->typeface_runs.push_back(std::move(typeface_run));
  }
  return text_run;
}

}  // namespace

TEST(PdfInkTextSplitTypefaceRunsTest, NoOffset) {
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
                                      {},
                                  },
                                  {}));

  std::vector<InkTextInfo> ink_info =
      InkTextInfo::SplitTypefaceRuns(text_runs, 10.0f);
  ASSERT_THAT(ink_info, SizeIs(5));

  EXPECT_THAT(
      ink_info[0],
      InkTextInfoEq(
          FontId(0),
          /*glyphs=*/std::vector<uint32_t>{1, 2, 3, 4, 5},
          /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f, 3.0f, 4.0f},
          /*location=*/gfx::RectF(10.0f, 10.0f, 5.0f, 2.0f),
          /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.0f},
                            /*location=*/gfx::RectF(15.0f, 10.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(
      ink_info[2],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 3.0f, 2.0f),
                    /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[3],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.0f},
                            /*location=*/gfx::RectF(13.0f, 20.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(
      ink_info[4],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.0f, 2.0f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 4.0f, 2.0f),
                    /*is_horizontal=*/true));
}

TEST(PdfInkTextSplitTypefaceRunsTest, HorizontalOffset) {
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
                      {},
                  },
                  {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
                    gfx::Vector2dF(3.0f, 0.0f)},
                   {}}));

  std::vector<InkTextInfo> ink_info =
      InkTextInfo::SplitTypefaceRuns(text_runs, 10.0f);
  ASSERT_THAT(ink_info, SizeIs(3));

  EXPECT_THAT(
      ink_info[0],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.5f, 1.6f},
                    /*location=*/gfx::RectF(10.0f, 20.0f, 3.0f, 2.0f),
                    /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(
      ink_info[2],
      InkTextInfoEq(FontId(0),
                    /*glyphs=*/std::vector<uint32_t>{1, 2, 3},
                    /*glyph_positions=*/std::vector<float>{0.0f, 1.9f, 2.7f},
                    /*location=*/gfx::RectF(9.6f, 20.0f, 4.0f, 2.0f),
                    /*is_horizontal=*/true));
}

TEST(PdfInkTextSplitTypefaceRunsTest, 2DOffset) {
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
                      {},
                  },
                  {{gfx::Vector2dF(-4.0f, 0.0f), gfx::Vector2dF(5.0f, 0.0f),
                    gfx::Vector2dF(3.0f, 5.0f)},
                   {}}));

  std::vector<InkTextInfo> ink_info =
      InkTextInfo::SplitTypefaceRuns(text_runs, 10.0f);
  ASSERT_THAT(ink_info, SizeIs(5));

  EXPECT_THAT(ink_info[0],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(10.0f, 20.5f, 1.4f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[1],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{2, 3},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.5f},
                            /*location=*/gfx::RectF(11.4f, 20.0f, 1.6f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[2],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{101, 102},
                            /*glyph_positions=*/std::vector<float>{0.0f, 0.3f},
                            /*location=*/gfx::RectF(13.5f, 20.0f, 2.0f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[3],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{1, 2},
                            /*glyph_positions=*/std::vector<float>{0.0f, 1.9f},
                            /*location=*/gfx::RectF(9.6f, 20.0f, 2.7f, 2.0f),
                            /*is_horizontal=*/true));

  EXPECT_THAT(ink_info[4],
              InkTextInfoEq(FontId(0),
                            /*glyphs=*/std::vector<uint32_t>{3},
                            /*glyph_positions=*/std::vector<float>{0.0f},
                            /*location=*/gfx::RectF(12.3f, 20.5f, 1.3f, 2.0f),
                            /*is_horizontal=*/true));
}

}  // namespace chrome_pdf

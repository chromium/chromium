// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_ink_text.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;
using testing::SizeIs;

namespace chrome_pdf {
namespace {

pdf::mojom::InkTextRunPtr MakeTextRun(
    gfx::RectF location,
    std::vector<std::vector<float>> typeface_run_total_advance) {
  auto text_run = pdf::mojom::InkTextRun::New();
  text_run->location = location;

  for (size_t i = 0; i < typeface_run_total_advance.size(); ++i) {
    const std::vector<float>& glyph_total_advance =
        typeface_run_total_advance[i];
    auto typeface_run = pdf::mojom::InkTypefaceRun::New();
    typeface_run->is_horizontal = true;
    for (size_t j = 0; j < glyph_total_advance.size(); ++j) {
      auto glyph = pdf::mojom::InkGlyphInfo::New();
      // Set the glyph IDs to 101, 102, 103... for the first typeface_run;
      // 201, 202, 203... for the second typeface_run; and so on.
      glyph->glyph = (j + 1) + (100 * i);
      glyph->total_advance = glyph_total_advance[j];
      typeface_run->glyphs.push_back(std::move(glyph));
    }
    text_run->typeface_runs.push_back(std::move(typeface_run));
  }
  return text_run;
}

}  // namespace

TEST(PdfInkTextTest, SplitTypefaceRuns) {
  std::vector<pdf::mojom::InkTextRunPtr> text_runs;
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 100.0f, 70.0f, 20.0f),
                                  /*typeface_run_total_advance=*/{
                                      {0.0f, 10.0f, 20.0f, 30.0f, 40.0f},
                                      {50.0f, 60.0f},
                                  }));
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 200.0f, 50.0f, 20.0f),
                                  /*typeface_run_total_advance=*/{
                                      {0.0f, 10.0f, 20.0f},
                                      {30.0f, 40.0f},
                                  }));
  text_runs.push_back(MakeTextRun(gfx::RectF(100.0f, 200.0f, 40.0f, 20.0f),
                                  /*typeface_run_total_advance=*/{
                                      {0.0f, 10.0f, 20.0f},
                                      {},
                                  }));

  std::vector<InkTextInfo> ink_info =
      InkTextInfo::SplitTypefaceRuns(text_runs, 10.0f);
  ASSERT_THAT(ink_info, SizeIs(5));

  EXPECT_THAT(ink_info[0].glyphs, ElementsAre(1, 2, 3, 4, 5));
  EXPECT_THAT(
      ink_info[0].glyph_positions,
      ElementsAre(gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(1.0f, 0.0f),
                  gfx::Vector2dF(2.0f, 0.0f), gfx::Vector2dF(3.0f, 0.0f),
                  gfx::Vector2dF(4.0f, 0.0f)));
  EXPECT_EQ(ink_info[0].location, gfx::RectF(10.0f, 10.0f, 5.0f, 2.0f));
  EXPECT_TRUE(ink_info[0].is_horizontal);

  EXPECT_THAT(ink_info[1].glyphs, ElementsAre(101, 102));
  EXPECT_THAT(
      ink_info[1].glyph_positions,
      ElementsAre(gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(1.0f, 0.0f)));
  EXPECT_EQ(ink_info[1].location, gfx::RectF(15.0f, 10.0f, 2.0f, 2.0f));
  EXPECT_TRUE(ink_info[1].is_horizontal);

  EXPECT_THAT(ink_info[2].glyphs, ElementsAre(1, 2, 3));
  EXPECT_THAT(
      ink_info[2].glyph_positions,
      ElementsAre(gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(1.0f, 0.0f),
                  gfx::Vector2dF(2.0f, 0.0f)));
  EXPECT_EQ(ink_info[2].location, gfx::RectF(10.0f, 20.0f, 3.0f, 2.0f));
  EXPECT_TRUE(ink_info[2].is_horizontal);

  EXPECT_THAT(ink_info[3].glyphs, ElementsAre(101, 102));
  EXPECT_THAT(
      ink_info[3].glyph_positions,
      ElementsAre(gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(1.0f, 0.0f)));
  EXPECT_EQ(ink_info[3].location, gfx::RectF(13.0f, 20.0f, 2.0f, 2.0f));
  EXPECT_TRUE(ink_info[3].is_horizontal);

  EXPECT_THAT(ink_info[4].glyphs, ElementsAre(1, 2, 3));
  EXPECT_THAT(
      ink_info[4].glyph_positions,
      ElementsAre(gfx::Vector2dF(0.0f, 0.0f), gfx::Vector2dF(1.0f, 0.0f),
                  gfx::Vector2dF(2.0f, 0.0f)));
  EXPECT_EQ(ink_info[4].location, gfx::RectF(10.0f, 20.0f, 4.0f, 2.0f));
  EXPECT_TRUE(ink_info[4].is_horizontal);
}

}  // namespace chrome_pdf

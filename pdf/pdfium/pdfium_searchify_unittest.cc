// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <algorithm>
#include <ostream>

#include "base/numerics/angle_conversions.h"
#include "base/strings/stringprintf.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"

namespace {

constexpr float kFloatTolerance = 0.00001f;

bool FloatNear(float a, float b, float abs_error) {
  return std::abs(a - b) <= abs_error;
}

}  // namespace

// Lets EXPECT_EQ() compare FS_MATRIX.
constexpr bool operator==(const FS_MATRIX& lhs, const FS_MATRIX& rhs) {
  return FloatNear(lhs.a, rhs.a, kFloatTolerance) &&
         FloatNear(lhs.b, rhs.b, kFloatTolerance) &&
         FloatNear(lhs.c, rhs.c, kFloatTolerance) &&
         FloatNear(lhs.d, rhs.d, kFloatTolerance) &&
         FloatNear(lhs.e, rhs.e, kFloatTolerance) &&
         FloatNear(lhs.f, rhs.f, kFloatTolerance);
}

// Lets EXPECT_EQ() automatically print out FS_MATRIX on failure. Similar to
// other PrintTo() functions in //ui/gfx/geometry.
void PrintTo(const FS_MATRIX& matrix, ::std::ostream* os) {
  *os << base::StringPrintf("%f,%f,%f,%f,%f,%f", matrix.a, matrix.b, matrix.c,
                            matrix.d, matrix.e, matrix.f);
}

namespace chrome_pdf {

TEST(PdfiumSearchifyTest, ConvertToPdfOrigin) {
  constexpr gfx::Rect kRect(100, 50, 20, 30);
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/0,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(100, 712), result.point);
    EXPECT_FLOAT_EQ(0, result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/45,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(78.786796f, 720.786796f), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-45), result.theta);
  }

  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(70, 742), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-90), result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/180,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(100, 772), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(-180), result.theta);
  }
  {
    SearchifyBoundingBoxOrigin result = ConvertToPdfOriginForTesting(
        /*rect=*/kRect,
        /*angle=*/-90,
        /*coordinate_system_height=*/792);
    EXPECT_EQ(gfx::PointF(130, 742), result.point);
    EXPECT_FLOAT_EQ(base::DegToRad<float>(90), result.theta);
  }
}

TEST(PdfiumSearchifyTest, CalculateWordMoveMatrix) {
  constexpr gfx::Rect kRect(100, 50, 20, 30);
  {
    // 0 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(100, 712), 0);
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(1, 0, 0, 1, 100, 712), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(-1, 0, 0, 1, 120, 712), matrix_rtl);
  }
  {
    // 45 degree case.
    const SearchifyBoundingBoxOrigin origin(
        gfx::PointF(78.786796f, 720.786796f), base::DegToRad<float>(-45));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0.707107f, -0.707107f, 0.707107f, 0.707107f, 78.786797f,
                        720.786804f),
              matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(-0.707107f, 0.707107f, 0.707107f, 0.707107f, 92.928932f,
                        706.644653f),
              matrix_rtl);
  }
  {
    // 90 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(70, 742),
                                            base::DegToRad<float>(-90));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0, -1, 1, 0, 70, 742), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(0, 1, 1, 0, 70, 722), matrix_rtl);
  }
  {
    // -90 degree case.
    const SearchifyBoundingBoxOrigin origin(gfx::PointF(130, 742),
                                            base::DegToRad<float>(90));
    FS_MATRIX matrix = CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                                         /*word_is_rtl=*/false);
    EXPECT_EQ(FS_MATRIX(0, 1, -1, 0, 130, 742), matrix);
    FS_MATRIX matrix_rtl =
        CalculateWordMoveMatrixForTesting(origin, kRect.width(),
                                          /*word_is_rtl=*/true);
    EXPECT_EQ(FS_MATRIX(0, -1, -1, 0, 130, 762), matrix_rtl);
  }
}

TEST(PdfiumSearchifyTest, GetSpaceRect) {
  // Horizontal, Left to Right.
  {
    gfx::Rect before(10, 10, 100, 10);
    gfx::Rect after(120, 10, 50, 10);
    gfx::Rect expected(110, 10, 10, 10);

    EXPECT_EQ(GetSpaceRectForTesting(before, after), expected);
  }

  // Horizontal, Right To Left.
  {
    gfx::Rect before(120, 10, 50, 10);
    gfx::Rect after(10, 10, 100, 10);
    gfx::Rect expected(110, 10, 10, 10);

    EXPECT_EQ(GetSpaceRectForTesting(before, after), expected);
  }

  // Vertical, Top to Bottom.
  {
    gfx::Rect before(10, 10, 10, 100);
    gfx::Rect after(10, 120, 10, 50);
    gfx::Rect expected(10, 110, 10, 10);

    EXPECT_EQ(GetSpaceRectForTesting(before, after), expected);
  }

  // Vertical, Bottom to Top.
  {
    gfx::Rect before(10, 120, 10, 50);
    gfx::Rect after(10, 10, 10, 100);
    gfx::Rect expected(10, 110, 10, 10);

    EXPECT_EQ(GetSpaceRectForTesting(before, after), expected);
  }

  // Empty rect.
  {
    gfx::Rect before(10, 10, 10, 10);
    gfx::Rect after;

    EXPECT_TRUE(GetSpaceRectForTesting(before, after).IsEmpty());
  }

  // Touching rects.
  {
    gfx::Rect before(10, 10, 10, 10);
    gfx::Rect after(20, 10, 10, 10);

    EXPECT_TRUE(GetSpaceRectForTesting(before, after).IsEmpty());
  }

  // Colliding rects.
  {
    gfx::Rect before(10, 10, 10, 10);
    gfx::Rect after(15, 10, 10, 10);

    EXPECT_TRUE(GetSpaceRectForTesting(before, after).IsEmpty());
  }

  // Covering rects, first in second.
  {
    gfx::Rect before(10, 10, 10, 10);
    gfx::Rect after(0, 0, 20, 20);

    EXPECT_TRUE(GetSpaceRectForTesting(before, after).IsEmpty());
  }

  // Covering rects, second in first.
  {
    gfx::Rect before(0, 0, 20, 20);
    gfx::Rect after(10, 10, 10, 10);

    EXPECT_TRUE(GetSpaceRectForTesting(before, after).IsEmpty());
  }
}

TEST(PdfiumSearchifyTest, AddingSpaceRects) {
  // No words.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);
    EXPECT_TRUE(words_and_spaces.empty());
  }

  // One word with space after it.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 50, 10);
    words.back()->has_space_after = true;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 1u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
  }

  // Two words with space between them.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 50, 10);
    words.back()->has_space_after = true;

    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word2";
    words.back()->bounding_box = gfx::Rect(60, 0, 50, 10);
    words.back()->has_space_after = false;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 3u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
    EXPECT_EQ(words_and_spaces[1].word, " ");
    EXPECT_EQ(words_and_spaces[2].word, words[1]->word);
  }

  // Two words with no space between them.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 50, 10);
    words.back()->has_space_after = false;

    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word2";
    words.back()->bounding_box = gfx::Rect(60, 0, 50, 10);
    words.back()->has_space_after = true;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 2u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
    EXPECT_EQ(words_and_spaces[1].word, words[1]->word);
  }

  // Two words with space between them and touching boundaries.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 50, 10);
    words.back()->has_space_after = true;

    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word2";
    words.back()->bounding_box = gfx::Rect(50, 0, 50, 10);
    words.back()->has_space_after = true;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 2u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
    EXPECT_EQ(words_and_spaces[1].word, words[1]->word);
  }

  // Two words with space between them, first one with empty bounding box.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 0, 0);
    words.back()->has_space_after = true;

    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word2";
    words.back()->bounding_box = gfx::Rect(60, 0, 50, 10);
    words.back()->has_space_after = false;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 2u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
    EXPECT_EQ(words_and_spaces[1].word, words[1]->word);
  }

  // Two words with space between them, second one with empty bounding box.
  {
    std::vector<screen_ai::mojom::WordBoxPtr> words;
    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word1";
    words.back()->bounding_box = gfx::Rect(0, 0, 50, 10);
    words.back()->has_space_after = true;

    words.emplace_back(screen_ai::mojom::WordBox::New());
    words.back()->word = "word2";
    words.back()->bounding_box = gfx::Rect(0, 0, 0, 0);
    words.back()->has_space_after = false;

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpacesForTesting(words);

    ASSERT_EQ(words_and_spaces.size(), 2u);
    EXPECT_EQ(words_and_spaces[0].word, words[0]->word);
    EXPECT_EQ(words_and_spaces[1].word, words[1]->word);
  }
}

}  // namespace chrome_pdf

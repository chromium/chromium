// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/manifest_icon_downloader.h"

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace content {

class ManifestIconDownloaderTest : public testing::TestWithParam<bool> {
 public:
  ManifestIconDownloaderTest(const ManifestIconDownloaderTest&) = delete;
  ManifestIconDownloaderTest& operator=(const ManifestIconDownloaderTest&) =
      delete;

 protected:
  ManifestIconDownloaderTest() : selects_square_only_(GetParam()) {}
  ~ManifestIconDownloaderTest() override = default;

  int width_to_height_ratio() {
    if (selects_square_only_)
      return 1;
    return ManifestIconDownloader::kMaxWidthToHeightRatio;
  }

  bool selects_square_only() { return selects_square_only_; }

  int FindBitmap(const int ideal_icon_size_in_px,
                 const int minimum_icon_size_in_px,
                 const std::vector<SkBitmap>& bitmaps) {
    return ManifestIconDownloader::FindClosestBitmapIndex(
        ideal_icon_size_in_px, minimum_icon_size_in_px, selects_square_only_,
        bitmaps);
  }

 private:
  bool selects_square_only_;
};

TEST_P(ManifestIconDownloaderTest, NoIcons) {
  ASSERT_EQ(-1, FindBitmap(0, 0, std::vector<SkBitmap>()));
}

TEST_P(ManifestIconDownloaderTest, ExactIsChosen) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10)};

  ASSERT_EQ(0, FindBitmap(10, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, BiggerIsChosen) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 20, 20)};

  ASSERT_EQ(0, FindBitmap(10, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, SmallerBelowMinimumIsIgnored) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10)};

  ASSERT_EQ(-1, FindBitmap(20, 15, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, SmallerAboveMinimumIsChosen) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 15, 15)};

  ASSERT_EQ(0, FindBitmap(20, 15, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, ExactIsPreferredOverBigger) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 20, 20),
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10)};

  ASSERT_EQ(1, FindBitmap(10, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, ExactIsPreferredOverSmaller) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 20, 20),
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10)};

  ASSERT_EQ(0, FindBitmap(20, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, BiggerIsPreferredOverCloserSmaller) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 20, 20),
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10)};

  ASSERT_EQ(0, FindBitmap(11, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, ClosestToExactIsChosen) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 25, 25),
      gfx::test::CreateBitmap(width_to_height_ratio() * 20, 20)};

  ASSERT_EQ(1, FindBitmap(10, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, MixedReturnsBiggestClosest) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10),
      gfx::test::CreateBitmap(width_to_height_ratio() * 8, 8),
      gfx::test::CreateBitmap(width_to_height_ratio() * 6, 6)};

  ASSERT_EQ(0, FindBitmap(9, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, MixedCanReturnMiddle) {
  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 10),
      gfx::test::CreateBitmap(width_to_height_ratio() * 8, 8),
      gfx::test::CreateBitmap(width_to_height_ratio() * 6, 6)};

  ASSERT_EQ(1, FindBitmap(7, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, SquareIsPickedOverNonSquare) {
  // The test applies to square only selection.
  if (!selects_square_only())
    return;

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 5, 5),
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 15)};

  ASSERT_EQ(0, FindBitmap(15, 5, bitmaps));
  ASSERT_EQ(0, FindBitmap(10, 5, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, MostSquareNonSquareIsPicked) {
  // The test applies to square only selection.
  if (!selects_square_only())
    return;

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap(width_to_height_ratio() * 25, 35),
      gfx::test::CreateBitmap(width_to_height_ratio() * 10, 11)};

  ASSERT_EQ(1, FindBitmap(25, 0, bitmaps));
  ASSERT_EQ(1, FindBitmap(35, 0, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, NonSquareBelowMinimumIsNotPicked) {
  std::vector<SkBitmap> bitmaps = {gfx::test::CreateBitmap(10, 15),
                                   gfx::test::CreateBitmap(15, 10)};

  ASSERT_EQ(-1, FindBitmap(15, 11, bitmaps));
}

TEST_P(ManifestIconDownloaderTest, ImproperWidthtoHeightRatioIsNotPicked) {
  // The test does not apply to square only selection.
  if (selects_square_only())
    return;

  std::vector<SkBitmap> bitmaps = {
      gfx::test::CreateBitmap((width_to_height_ratio() + 1) * 15, 15)};

  ASSERT_EQ(-1, FindBitmap(15, 11, bitmaps));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ManifestIconDownloaderTest,
                         ::testing::Bool());

}  // namespace content

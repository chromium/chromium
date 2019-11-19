// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using Purpose = blink::Manifest::ImageResource::Purpose;

namespace {
const int kIdealIconSize = 144;
const int kMinimumIconSize = 0;
// The same value as content::ManifestIconDownloader::kMaxWidthToHeightRatio
const int kMaxWidthToHeightRatio = 5;
}  // anonymous namespace

class ManifestIconSelectorTest : public testing::TestWithParam<bool> {
 public:
  ManifestIconSelectorTest() : selects_square_only_(GetParam()) {}
  ~ManifestIconSelectorTest() = default;

 protected:
  blink::Manifest::ImageResource CreateIcon(const std::string& url,
                                            const std::string& type,
                                            const std::vector<gfx::Size> sizes,
                                            Purpose purpose) {
    blink::Manifest::ImageResource icon;
    icon.src = GURL(url);
    icon.type = base::UTF8ToUTF16(type);
    icon.sizes = sizes;
    icon.purpose.push_back(purpose);

    return icon;
  }

  bool selects_square_only() { return selects_square_only_; }

  int width_to_height_ratio() {
    if (selects_square_only_)
      return 1;
    return kMaxWidthToHeightRatio;
  }

  GURL FindBestMatchingIcon(
      const std::vector<blink::Manifest::ImageResource>& icons,
      int ideal_icon_size_in_px,
      int minimum_icon_size_in_px,
      blink::Manifest::ImageResource::Purpose purpose) {
    if (selects_square_only_) {
      return ManifestIconSelector::FindBestMatchingSquareIcon(
          icons, ideal_icon_size_in_px, minimum_icon_size_in_px, purpose);
    }
    return ManifestIconSelector::FindBestMatchingIcon(
        icons, ideal_icon_size_in_px, minimum_icon_size_in_px,
        kMaxWidthToHeightRatio, purpose);
  }

 private:
  bool selects_square_only_;
};

TEST_P(ManifestIconSelectorTest, NoIcons) {
  // No icons should return the empty URL.
  std::vector<blink::Manifest::ImageResource> icons;
  GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                  Purpose::ANY);
  EXPECT_TRUE(url.is_empty());
}

TEST_P(ManifestIconSelectorTest, NoSizes) {
  // Icon with no sizes are ignored.
  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(CreateIcon("http://foo.com/icon.png", "",
                             std::vector<gfx::Size>(), Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                  Purpose::ANY);
  EXPECT_TRUE(url.is_empty());
}

TEST_P(ManifestIconSelectorTest, MIMETypeFiltering) {
  // Icons with type specified to a MIME type that isn't a valid image MIME type
  // are ignored.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(width_to_height_ratio() * 1024, 1024));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/foo_bar", sizes,
                             Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/", sizes, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/", sizes, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "video/mp4", sizes, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                  Purpose::ANY);
  EXPECT_TRUE(url.is_empty());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/png", sizes, Purpose::ANY));
  url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                             Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/gif", sizes, Purpose::ANY));
  url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                             Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/jpeg", sizes, Purpose::ANY));
  url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                             Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, PurposeFiltering) {
  // Icons with purpose specified to non-matching purpose are ignored.
  std::vector<gfx::Size> sizes_48;
  sizes_48.push_back(gfx::Size(width_to_height_ratio() * 48, 48));

  std::vector<gfx::Size> sizes_96;
  sizes_96.push_back(gfx::Size(width_to_height_ratio() * 96, 96));

  std::vector<gfx::Size> sizes_144;
  sizes_144.push_back(gfx::Size(width_to_height_ratio() * 144, 144));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_48.png", "", sizes_48, Purpose::BADGE));
  icons.push_back(
      CreateIcon("http://foo.com/icon_96.png", "", sizes_96, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_144.png", "", sizes_144, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, 48, kMinimumIconSize, Purpose::BADGE);
  EXPECT_EQ("http://foo.com/icon_48.png", url.spec());

  url = FindBestMatchingIcon(icons, 48, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_96.png", url.spec());

  url = FindBestMatchingIcon(icons, 96, kMinimumIconSize, Purpose::BADGE);
  EXPECT_EQ("http://foo.com/icon_48.png", url.spec());

  url = FindBestMatchingIcon(icons, 96, 96, Purpose::BADGE);
  EXPECT_TRUE(url.is_empty());

  url = FindBestMatchingIcon(icons, 144, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_144.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, IdealSizeIsUsedFirst) {
  // Each icon is marked with sizes that match the ideal icon size.
  std::vector<gfx::Size> sizes_48;
  sizes_48.push_back(gfx::Size(width_to_height_ratio() * 48, 48));

  std::vector<gfx::Size> sizes_96;
  sizes_96.push_back(gfx::Size(width_to_height_ratio() * 96, 96));

  std::vector<gfx::Size> sizes_144;
  sizes_144.push_back(gfx::Size(width_to_height_ratio() * 144, 144));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_48.png", "", sizes_48, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_96.png", "", sizes_96, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_144.png", "", sizes_144, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, 48, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_48.png", url.spec());

  url = FindBestMatchingIcon(icons, 96, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_96.png", url.spec());

  url = FindBestMatchingIcon(icons, 144, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_144.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, FirstIconWithIdealSizeIsUsedFirst) {
  // This test has three icons. The first icon is going to be used because it
  // contains the ideal size.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(
      gfx::Size(width_to_height_ratio() * kIdealIconSize, kIdealIconSize));
  sizes_1.push_back(gfx::Size(width_to_height_ratio() * kIdealIconSize * 2,
                              kIdealIconSize * 2));
  sizes_1.push_back(gfx::Size(width_to_height_ratio() * kIdealIconSize * 3,
                              kIdealIconSize * 3));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(width_to_height_ratio() * 1024, 1024));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(width_to_height_ratio() * 1024, 1024));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_x1.png", "", sizes_1, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x2.png", "", sizes_2, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x3.png", "", sizes_3, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                  Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, kIdealIconSize * 2, kMinimumIconSize,
                             Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, kIdealIconSize * 3, kMinimumIconSize,
                             Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, FallbackToSmallestLargerIcon) {
  // If there is no perfect icon, the smallest larger icon will be chosen.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(width_to_height_ratio() * 90, 90));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(width_to_height_ratio() * 128, 128));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(width_to_height_ratio() * 192, 192));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_x1.png", "", sizes_1, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x2.png", "", sizes_2, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x3.png", "", sizes_3, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, 48, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, 96, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x2.png", url.spec());

  url = FindBestMatchingIcon(icons, 144, kMinimumIconSize, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, FallbackToLargestIconLargerThanMinimum) {
  // When an icon of the correct size has not been found, we fall back to the
  // closest non-matching sizes. Make sure that the minimum passed is enforced.
  std::vector<gfx::Size> sizes_1_2;
  std::vector<gfx::Size> sizes_3;

  sizes_1_2.push_back(gfx::Size(width_to_height_ratio() * 47, 47));
  sizes_3.push_back(gfx::Size(width_to_height_ratio() * 95, 95));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_x1.png", "", sizes_1_2, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x2.png", "", sizes_1_2, Purpose::ANY));
  icons.push_back(
      CreateIcon("http://foo.com/icon_x3.png", "", sizes_3, Purpose::ANY));

  // Icon 3 should match.
  GURL url = FindBestMatchingIcon(icons, 1024, 48, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());

  // Nothing matches here as the minimum is 96.
  url = FindBestMatchingIcon(icons, 1024, 96, Purpose::ANY);
  EXPECT_TRUE(url.is_empty());
}

TEST_P(ManifestIconSelectorTest, IdealVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(width_to_height_ratio() * 2, 2));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_x1.png", "", sizes, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, 2, 1, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, SizeVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(width_to_height_ratio() * 2, 2));

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon_x1.png", "", sizes, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, 200, 1, Purpose::ANY);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_P(ManifestIconSelectorTest, IconsWithInvalidDimensionsAreIgnored) {
  std::vector<gfx::Size> sizes;
  if (selects_square_only()) {
    // Square selector should ignore non-square icons.
    sizes.push_back(gfx::Size(1024, 1023));
  } else {
    // Landscape selector should ignore icons with improper width/height ratio.
    sizes.push_back(gfx::Size((kMaxWidthToHeightRatio + 1) * 1023, 1023));
    // Landscape selector should ignore portrait icons.
    sizes.push_back(gfx::Size(1023, 1024));
  }

  std::vector<blink::Manifest::ImageResource> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "", sizes, Purpose::ANY));

  GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                  Purpose::ANY);
  EXPECT_TRUE(url.is_empty());
}

TEST_P(ManifestIconSelectorTest, ClosestIconToIdeal) {
  // Ensure ManifestIconSelector::FindBestMatchingSquareIcon selects the closest
  // icon to the ideal size when presented with a number of options.
  int very_small = kIdealIconSize / 4;
  int small_size = kIdealIconSize / 2;
  int bit_small = kIdealIconSize - 1;
  int bit_big = kIdealIconSize + 1;
  int big = kIdealIconSize * 2;
  int very_big = kIdealIconSize * 4;

  // (very_small, bit_small) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(
        gfx::Size(width_to_height_ratio() * bit_small, bit_small));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_small, bit_small, small_size) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(
        gfx::Size(width_to_height_ratio() * bit_small, bit_small));

    std::vector<gfx::Size> sizes_3;
    sizes_3.push_back(
        gfx::Size(width_to_height_ratio() * small_size, small_size));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no_1.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon_no_2.png", "", sizes_3, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_big, big) => big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(width_to_height_ratio() * very_big, very_big));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(width_to_height_ratio() * big, big));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_big, big, bit_big) => bit_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(width_to_height_ratio() * very_big, very_big));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(width_to_height_ratio() * big, big));

    std::vector<gfx::Size> sizes_3;
    sizes_3.push_back(gfx::Size(width_to_height_ratio() * bit_big, bit_big));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_2, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_3, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (bit_small, very_big) => very_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * bit_small, bit_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(width_to_height_ratio() * very_big, very_big));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (bit_small, bit_big) => bit_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * bit_small, bit_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(width_to_height_ratio() * bit_big, bit_big));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}

TEST_P(ManifestIconSelectorTest, UseAnyIfNoIdealSize) {
  // 'any' (ie. gfx::Size(0,0)) should be used if there is no icon of a
  // ideal size.

  // Icon with 'any' and icon with ideal size => ideal size is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * kIdealIconSize, kIdealIconSize));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Icon with 'any' and icon larger than ideal size => any is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(width_to_height_ratio() * (kIdealIconSize + 1),
                                kIdealIconSize + 1));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Multiple icons with 'any' => the last one is chosen.
  {
    std::vector<gfx::Size> sizes;
    sizes.push_back(gfx::Size(0, 0));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon_no1.png", "", sizes, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon_no2.png", "", sizes, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize * 3, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Multiple icons with ideal size => the last one is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(
        gfx::Size(width_to_height_ratio() * kIdealIconSize, kIdealIconSize));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(
        gfx::Size(width_to_height_ratio() * kIdealIconSize, kIdealIconSize));

    std::vector<blink::Manifest::ImageResource> icons;
    icons.push_back(
        CreateIcon("http://foo.com/icon.png", "", sizes_1, Purpose::ANY));
    icons.push_back(
        CreateIcon("http://foo.com/icon_no.png", "", sizes_2, Purpose::ANY));

    GURL url = FindBestMatchingIcon(icons, kIdealIconSize, kMinimumIconSize,
                                    Purpose::ANY);
    EXPECT_EQ("http://foo.com/icon_no.png", url.spec());
  }
}

INSTANTIATE_TEST_SUITE_P(/* No prefix */,
                         ManifestIconSelectorTest,
                         ::testing::Bool());

}  // namespace blink

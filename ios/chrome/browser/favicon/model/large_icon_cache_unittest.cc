// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/model/large_icon_cache.h"

#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "skia/ext/skia_utils_ios.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace {

const char kDummyUrl[] = "http://www.example.com";
const char kDummyUrl2[] = "http://www.example2.com";
const SkColor kTestColor = SK_ColorRED;

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w,
                                                      int h,
                                                      SkColor color) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  // Create bitmap and fill with `color`.
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  gfx::PNGCodec::EncodeBGRASkBitmap(gfx::test::CreateBitmap(w, h, color), false,
                                    &data->as_vector());
  result.bitmap_data = data;

  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyUrl);
  result.icon_type = favicon_base::IconType::kTouchIcon;
  CHECK(result.is_valid());
  return result;
}

class LargeIconCacheTest : public PlatformTest {
 public:
  LargeIconCacheTest() {
    expected_fallback_icon_style_.reset(new favicon_base::FallbackIconStyle());
    expected_fallback_icon_style_->background_color = kTestColor;
    expected_fallback_icon_style_->is_default_background_color = false;
    expected_bitmap_ = CreateTestBitmap(24, 24, kTestColor);
    large_icon_cache_.reset(new LargeIconCache);
  }

  LargeIconCacheTest(const LargeIconCacheTest&) = delete;
  LargeIconCacheTest& operator=(const LargeIconCacheTest&) = delete;

  ~LargeIconCacheTest() override {}

 protected:
  std::unique_ptr<LargeIconCache> large_icon_cache_;
  favicon_base::FaviconRawBitmapResult expected_bitmap_;
  std::unique_ptr<favicon_base::FallbackIconStyle>
      expected_fallback_icon_style_;

  bool is_callback_invoked_;
};

TEST_F(LargeIconCacheTest, EmptyCache) {
  std::unique_ptr<LargeIconCache> large_icon_cache(new LargeIconCache);
  EXPECT_EQ(nullptr, large_icon_cache->GetCachedResult(GURL(kDummyUrl)));
}

TEST_F(LargeIconCacheTest, RetreiveItem) {
  std::unique_ptr<favicon_base::LargeIconResult> expected_result1;
  std::unique_ptr<favicon_base::LargeIconResult> expected_result2;
  expected_result1.reset(new favicon_base::LargeIconResult(expected_bitmap_));
  expected_result2.reset(new favicon_base::LargeIconResult(
      new favicon_base::FallbackIconStyle(*expected_fallback_icon_style_)));

  large_icon_cache_->SetCachedResult(GURL(kDummyUrl), *expected_result1);
  large_icon_cache_->SetCachedResult(GURL(kDummyUrl2), *expected_result2);

  std::unique_ptr<favicon_base::LargeIconResult> result1 =
      large_icon_cache_->GetCachedResult(GURL(kDummyUrl));
  EXPECT_EQ(true, result1->bitmap.is_valid());
  EXPECT_EQ(expected_result1->bitmap.pixel_size, result1->bitmap.pixel_size);

  std::unique_ptr<favicon_base::LargeIconResult> result2 =
      large_icon_cache_->GetCachedResult(GURL(kDummyUrl2));
  EXPECT_EQ(false, result2->bitmap.is_valid());
  EXPECT_EQ(expected_result2->fallback_icon_style->background_color,
            result2->fallback_icon_style->background_color);
  EXPECT_FALSE(result2->fallback_icon_style->is_default_background_color);

  // Test overwriting kDummyUrl.
  large_icon_cache_->SetCachedResult(GURL(kDummyUrl), *expected_result2);
  std::unique_ptr<favicon_base::LargeIconResult> result3 =
      large_icon_cache_->GetCachedResult(GURL(kDummyUrl2));
  EXPECT_EQ(false, result3->bitmap.is_valid());
  EXPECT_EQ(expected_result2->fallback_icon_style->background_color,
            result3->fallback_icon_style->background_color);
  EXPECT_FALSE(result2->fallback_icon_style->is_default_background_color);
}

}  // namespace

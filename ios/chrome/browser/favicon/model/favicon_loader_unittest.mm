// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_loader.h"

#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "components/favicon_base/favicon_types.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/skia/include/core/SkBitmap.h"
#import "ui/gfx/codec/png_codec.h"
#import "url/gurl.h"

namespace {

// Dummy URL for the favicon case.
const char kTestFaviconURL[] = "http://test/favicon";
// Dummy URL for the fallback case.
const char kTestFallbackURL[] = "http://test/fallback";

// Size of dummy favicon image.
const CGFloat kTestFaviconSize = 57;

// FaviconLoaderTest is parameterized on this enum to test both
// FaviconLoader::FaviconForPageUrl and FaviconLoader::FaviconForIconUrl.
enum FaviconUrlType { TEST_PAGE_URL, TEST_ICON_URL };

// FakeLargeIconService mimics a LargeIconService that returns a LargeIconResult
// with a test favicon image for kTestFaviconURL and a LargeIconResult
// initialized with FallbackIconStyle.
class FakeLargeIconService : public favicon::LargeIconServiceImpl {
 public:
  FakeLargeIconService()
      : favicon::LargeIconServiceImpl(
            /*favicon_service=*/nullptr,
            /*image_fetcher=*/nullptr,
            /*desired_size_in_dip_for_server_requests=*/0,
            /*icon_type_for_server_requests=*/
            favicon_base::IconType::kTouchIcon,
            /*google_server_client_param=*/"test_chrome") {}

  // Returns LargeIconResult with valid bitmap if `page_url` is
  // `kTestFaviconURL`, or LargeIconResult with fallback style.
  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      const GURL& page_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    if (page_url.spec() == kTestFaviconURL) {
      favicon_base::FaviconRawBitmapResult bitmapResult;
      bitmapResult.expired = false;

      // Create bitmap.
      auto data = base::MakeRefCounted<base::RefCountedBytes>();
      SkBitmap bitmap;
      bitmap.allocN32Pixels(30, 30);
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &data->as_vector());
      bitmapResult.bitmap_data = data;

      favicon_base::LargeIconResult result(bitmapResult);
      std::move(callback).Run(result);
    } else {
      favicon_base::FallbackIconStyle* fallback =
          new favicon_base::FallbackIconStyle();
      favicon_base::LargeIconResult result(fallback);
      fallback = NULL;
      std::move(callback).Run(result);
    }

    return 1;
  }

  // Returns the same as `GetLargeIconRawBitmapOrFallbackStyleForPageUrl`.
  base::CancelableTaskTracker::TaskId
  GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
      const GURL& icon_url,
      int min_source_size_in_pixel,
      int desired_size_in_pixel,
      favicon_base::LargeIconCallback callback,
      base::CancelableTaskTracker* tracker) override {
    return GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
        icon_url, min_source_size_in_pixel, desired_size_in_pixel,
        std::move(callback), tracker);
  }
};

class FaviconLoaderTest : public PlatformTest,
                          public ::testing::WithParamInterface<FaviconUrlType> {
 public:
  FaviconLoaderTest(const FaviconLoaderTest&) = delete;
  FaviconLoaderTest& operator=(const FaviconLoaderTest&) = delete;

 protected:
  FaviconLoaderTest() : favicon_loader_(&large_icon_service_) {}

  FakeLargeIconService large_icon_service_;
  FaviconLoader favicon_loader_;

  // Returns FaviconLoader::FaviconForPageUrl or
  // FaviconLoader::FaviconForIconUrl depending on the TEST_P param.
  void FaviconForUrl(const GURL& url,
                     FaviconLoader::FaviconAttributesCompletionBlock callback) {
    if (GetParam() == TEST_PAGE_URL) {
      favicon_loader_.FaviconForPageUrl(url, kTestFaviconSize, kTestFaviconSize,
                                        /*fallback_to_google_server=*/false,
                                        callback);
    } else {
      favicon_loader_.FaviconForIconUrl(url, kTestFaviconSize, kTestFaviconSize,
                                        callback);
    }
  }
};

// Tests that image is returned when a favicon is retrieved from
// LargeIconService.
TEST_P(FaviconLoaderTest, FaviconForPageUrl) {
  __block bool callback_executed = false;
  auto confirmation_block = ^(FaviconAttributes* favicon_attributes) {
    callback_executed = true;
    EXPECT_TRUE(favicon_attributes.faviconImage);
  };
  FaviconForUrl(GURL(kTestFaviconURL), confirmation_block);
  EXPECT_TRUE(callback_executed);
}

// Tests that fallback data is provided when no favicon is retrieved from
// LargeIconService.
TEST_P(FaviconLoaderTest, FallbackIcon) {
  __block int callback_executed_count = 0;
  auto confirmation_block = ^(FaviconAttributes* favicon_attributes) {
    if (callback_executed_count == 0) {
      // Check that a placeholder image is received.
      EXPECT_TRUE(favicon_attributes.faviconImage);
    } else {
      // Check that a monogram is used as a fallback.
      EXPECT_FALSE(favicon_attributes.faviconImage);
      EXPECT_TRUE(favicon_attributes.monogramString);
      EXPECT_TRUE(favicon_attributes.textColor);
      EXPECT_TRUE(favicon_attributes.backgroundColor);
    }

    ++callback_executed_count;
  };
  FaviconForUrl(GURL(kTestFallbackURL), confirmation_block);
  EXPECT_EQ(callback_executed_count, 2);
}

// Tests that when favicon is in cache, the callback is synchronously called.
TEST_P(FaviconLoaderTest, Cache) {
  // Favicon retrieval that should put it in the cache.
  FaviconForUrl(GURL(kTestFaviconURL), ^(FaviconAttributes* attributes){
                });
  __block bool callback_executed = false;
  __block UIImage* faviconImage = nil;
  __block int callback_executed_count = 0;
  auto confirmation_block = ^(FaviconAttributes* faviconAttributes) {
    callback_executed = true;
    faviconImage = faviconAttributes.faviconImage;
    ++callback_executed_count;
  };
  FaviconForUrl(GURL(kTestFaviconURL), confirmation_block);
  EXPECT_EQ(callback_executed_count, 1);
  // The callback should be immediately executed.
  EXPECT_TRUE(callback_executed);
  // The cached image should be available immediately.
  EXPECT_TRUE(faviconImage);
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticFaviconLoaderTest,
                         FaviconLoaderTest,
                         ::testing::Values(FaviconUrlType::TEST_PAGE_URL,
                                           FaviconUrlType::TEST_ICON_URL));

}  // namespace

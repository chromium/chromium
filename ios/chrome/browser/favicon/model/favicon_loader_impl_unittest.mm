// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_loader_impl.h"

#import "base/functional/callback_helpers.h"
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

// FaviconLoaderImplTest is parameterized on this enum to test both
// FaviconLoaderImpl::FaviconForPageUrl and
// FaviconLoaderImpl::FaviconForIconUrl.
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
      SkBitmap bitmap;
      bitmap.allocN32Pixels(30, 30);
      std::optional<std::vector<uint8_t>> data =
          gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                            /*discard_transparency=*/false);
      bitmapResult.bitmap_data =
          base::MakeRefCounted<base::RefCountedBytes>(std::move(data.value()));

      favicon_base::LargeIconResult result(bitmapResult);
      std::move(callback).Run(result);
    } else {
      auto fallback = std::make_unique<favicon_base::FallbackIconStyle>();
      favicon_base::LargeIconResult result(std::move(fallback));
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

class FaviconLoaderImplTest
    : public PlatformTest,
      public ::testing::WithParamInterface<FaviconUrlType> {
 public:
  // Callback passed to FaviconForUrl(...).
  using Callback = base::RepeatingCallback<void(FaviconAttributes*)>;

  FaviconLoaderImplTest() : favicon_loader_(&large_icon_service_) {}

  FaviconLoaderImplTest(const FaviconLoaderImplTest&) = delete;
  FaviconLoaderImplTest& operator=(const FaviconLoaderImplTest&) = delete;

  // Invokes FaviconForPageUrl(...) or FaviconForIconUrl(...) depending on
  // the TEST_P param. Since FakeLargeIconService is synchronous, the method
  // itself is also synchronous.
  void FaviconForUrl(const GURL& url, const Callback& callback) {
    if (GetParam() == TEST_PAGE_URL) {
      favicon_loader_.FaviconForPageUrl(url, kTestFaviconSize, kTestFaviconSize,
                                        /*fallback_to_google_server=*/false,
                                        base::CallbackToBlock(callback));
    } else {
      favicon_loader_.FaviconForIconUrl(url, kTestFaviconSize, kTestFaviconSize,
                                        base::CallbackToBlock(callback));
    }
  }

 private:
  FakeLargeIconService large_icon_service_;
  FaviconLoaderImpl favicon_loader_;
};

// Tests that image is returned when a favicon is retrieved from
// LargeIconService.
TEST_P(FaviconLoaderImplTest, FaviconForPageUrl) {
  int call_count = 0;
  FaviconForUrl(GURL(kTestFaviconURL),
                base::BindRepeating(
                    [](int& counter, FaviconAttributes* attrs) {
                      ++counter;
                      EXPECT_TRUE(attrs.faviconImage);
                    },
                    std::ref(call_count)));
  EXPECT_GE(call_count, 1);
}

// Tests that fallback data is provided when no favicon is retrieved from
// LargeIconService.
TEST_P(FaviconLoaderImplTest, FallbackIcon) {
  int call_count = 0;
  FaviconForUrl(GURL(kTestFallbackURL),
                base::BindRepeating(
                    [](int& counter, FaviconAttributes* attrs) {
                      ++counter;
                      if (counter == 1) {
                        // Check that a placeholder image is received.
                        EXPECT_TRUE(attrs.faviconImage);
                      } else if (counter == 2) {
                        // Check that a monogram is used as a fallback.
                        EXPECT_FALSE(attrs.faviconImage);
                        EXPECT_TRUE(attrs.monogramString);
                        EXPECT_TRUE(attrs.textColor);
                        EXPECT_TRUE(attrs.backgroundColor);
                      }
                    },
                    std::ref(call_count)));
  EXPECT_EQ(call_count, 2);
}

// Tests that when favicon is in cache, the callback is synchronously called.
TEST_P(FaviconLoaderImplTest, Cache) {
  // Favicon retrieval that should put it in the cache.
  {
    int call_count = 0;
    FaviconForUrl(GURL(kTestFaviconURL),
                  base::BindRepeating(
                      [](int& counter, FaviconAttributes* attrs) { ++counter; },
                      std::ref(call_count)));
    ASSERT_EQ(call_count, 2);
  }

  // The callback should be immediately executed.
  int call_count = 0;
  FaviconForUrl(GURL(kTestFaviconURL),
                base::BindRepeating(
                    [](int& counter, FaviconAttributes* attrs) {
                      ++counter;
                      EXPECT_TRUE(attrs.faviconImage);
                    },
                    std::ref(call_count)));
  EXPECT_EQ(call_count, 1);
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticFaviconLoaderImplTest,
                         FaviconLoaderImplTest,
                         ::testing::Values(FaviconUrlType::TEST_PAGE_URL,
                                           FaviconUrlType::TEST_ICON_URL));

}  // namespace

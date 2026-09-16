// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_util.h"

#import "base/values.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

using FaviconUtilTest = PlatformTest;

// Tries to extract multiple favicons url, all should be extracted.
TEST_F(FaviconUtilTest, ExtractFaviconURLMultipleFavicons) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon")
                      .Set("sizes", "10x20"))
          .Append(base::DictValue()
                      .Set("href", "http://fav2.ico")
                      .Set("rel", "apple-touch-icon")
                      .Set("sizes", "10x20 30x40"))
          .Append(base::DictValue()
                      .Set("href", "http://fav3.ico")
                      .Set("rel", "apple-touch-icon-precomposed")
                      .Set("sizes", "werfxw"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(3U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  ASSERT_EQ(1U, urls[0].icon_sizes.size());
  EXPECT_EQ(gfx::Size(10, 20), urls[0].icon_sizes[0]);

  EXPECT_EQ(GURL("http://fav2.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchIcon, urls[1].icon_type);
  ASSERT_EQ(2U, urls[1].icon_sizes.size());
  EXPECT_EQ(gfx::Size(10, 20), urls[1].icon_sizes[0]);
  EXPECT_EQ(gfx::Size(30, 40), urls[1].icon_sizes[1]);

  EXPECT_EQ(GURL("http://fav3.ico"), urls[2].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchPrecomposedIcon,
            urls[2].icon_type);
  EXPECT_EQ(0U, urls[2].icon_sizes.size());
}

// Tries to extract favicons with the rel attributes missing in one of them.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoRel) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue().Set("href", "http://fav2.ico"))
          .Append(base::DictValue()
                      .Set("href", "http://fav3.ico")
                      .Set("rel", "apple-touch-icon-precomposed"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav3.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchPrecomposedIcon,
            urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

// Tries to extract favicons with the rel attributes being an int.
TEST_F(FaviconUtilTest, ExtractFaviconURLIntRel) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "http://fav2.ico")
                      .Set("rel", 12345))
          .Append(base::DictValue()
                      .Set("href", "http://fav3.ico")
                      .Set("rel", "apple-touch-icon-precomposed"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav3.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchPrecomposedIcon,
            urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

// Tries to extract favicons with the href attributes missing in one of them.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoHref) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue().Set("rel", "apple-touch-icon"))
          .Append(base::DictValue()
                      .Set("href", "http://fav3.ico")
                      .Set("rel", "apple-touch-icon-precomposed"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav3.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchPrecomposedIcon,
            urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

// Tries to extract favicons with the href attributes is an invalid url.
TEST_F(FaviconUtilTest, ExtractFaviconURLHrefNotValidURL) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "this is not a valid url")
                      .Set("rel", "apple-touch-icon"))
          .Append(base::DictValue()
                      .Set("href", "http://fav3.ico")
                      .Set("rel", "apple-touch-icon-precomposed"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav3.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchPrecomposedIcon,
            urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

// Tries to extract the default favicon when there are no favicon in the
// message.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoFavicons) {
  const base::ListValue favicons;

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://chromium.org/favicon.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());
}

// Tries to extract favicons with the sizes attributes containing one correct
// size and one incorrectly formatted.
TEST_F(FaviconUtilTest, ExtractFaviconURLSizesCorrectAndGarbage) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon")
                      .Set("sizes", "10x20 sgxer"))
          .Append(base::DictValue()
                      .Set("href", "http://fav2.ico")
                      .Set("rel", "apple-touch-icon")
                      .Set("sizes", "sgxer 30x40"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);

  ASSERT_EQ(1U, urls[0].icon_sizes.size());
  EXPECT_EQ(10, urls[0].icon_sizes[0].width());
  EXPECT_EQ(20, urls[0].icon_sizes[0].height());

  EXPECT_EQ(GURL("http://fav2.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchIcon, urls[1].icon_type);

  ASSERT_EQ(1U, urls[1].icon_sizes.size());
  EXPECT_EQ(30, urls[1].icon_sizes[0].width());
  EXPECT_EQ(40, urls[1].icon_sizes[0].height());
}

// Tries to extract favicons with the sizes attributes containing size only
// partially correctly formatted.
TEST_F(FaviconUtilTest, ExtractFaviconURLSizesPartiallyCorrect) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon")
                      .Set("sizes", "10x"))
          .Append(base::DictValue()
                      .Set("href", "http://fav2.ico")
                      .Set("rel", "apple-touch-icon")
                      .Set("sizes", "x40"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav2.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchIcon, urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

// Tries to extract favicons from various origins from a normal orign.
TEST_F(FaviconUtilTest, ExtractFaviconURLFromNormalOrigin) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "https://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "data:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "file:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "testwebui:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "non-webui:favicon.ico")
                      .Set("rel", "icon"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"));

  ASSERT_EQ(3U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("https://fav.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());

  EXPECT_EQ(GURL("data:favicon.ico"), urls[2].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[2].icon_type);
  EXPECT_EQ(0U, urls[2].icon_sizes.size());
}

// Tries to extract favicons from various origins from a custom orign.
TEST_F(FaviconUtilTest, ExtractFaviconURLFromCustomOrigin) {
  const base::ListValue favicons =
      base::ListValue()
          .Append(base::DictValue()
                      .Set("href", "http://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "https://fav.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "data:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "file:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "testwebui:favicon.ico")
                      .Set("rel", "icon"))
          .Append(base::DictValue()
                      .Set("href", "non-webui:favicon.ico")
                      .Set("rel", "icon"));

  const std::vector<web::FaviconURL> urls =
      web::ExtractFaviconURL(favicons, GURL("testwebui://internal-ui"));

  ASSERT_EQ(4U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("https://fav.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());

  EXPECT_EQ(GURL("data:favicon.ico"), urls[2].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[2].icon_type);
  EXPECT_EQ(0U, urls[2].icon_sizes.size());

  EXPECT_EQ(GURL("testwebui:favicon.ico"), urls[3].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[3].icon_type);
  EXPECT_EQ(0U, urls[3].icon_sizes.size());
}

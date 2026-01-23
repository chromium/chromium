// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_util.h"

#import "base/values.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace web {

using FaviconUtilTest = PlatformTest;

// Tries to extract multiple favicons url, all should be extracted.
TEST_F(FaviconUtilTest, ExtractFaviconURLMultipleFavicons) {
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  favicon.Set("sizes", "10x20");
  base::DictValue favicon2;
  favicon2.Set("href", "http://fav2.ico");
  favicon2.Set("rel", "apple-touch-icon");
  favicon2.Set("sizes", "10x20 30x40");
  base::DictValue favicon3;
  favicon3.Set("href", "http://fav3.ico");
  favicon3.Set("rel", "apple-touch-icon-precomposed");
  favicon3.Set("sizes", "werfxw");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));
  favicons.Append(std::move(favicon3));

  std::vector<web::FaviconURL> urls;
  bool result =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"), &urls);

  EXPECT_TRUE(result);
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
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  base::DictValue favicon2;
  favicon2.Set("href", "http://fav2.ico");
  base::DictValue favicon3;
  favicon3.Set("href", "http://fav3.ico");
  favicon3.Set("rel", "apple-touch-icon-precomposed");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));
  favicons.Append(std::move(favicon3));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(favicons, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract favicons with the rel attributes being an int.
TEST_F(FaviconUtilTest, ExtractFaviconURLIntRel) {
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  base::DictValue favicon2;
  favicon2.Set("href", "http://fav2.ico");
  favicon2.Set("rel", 12);
  base::DictValue favicon3;
  favicon3.Set("href", "http://fav3.ico");
  favicon3.Set("rel", "apple-touch-icon-precomposed");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));
  favicons.Append(std::move(favicon3));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(favicons, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract favicons with the href attributes missing in one of them.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoHref) {
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  base::DictValue favicon2;
  favicon2.Set("rel", "apple-touch-icon");
  base::DictValue favicon3;
  favicon3.Set("href", "http://fav3.ico");
  favicon3.Set("rel", "apple-touch-icon-precomposed");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));
  favicons.Append(std::move(favicon3));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(favicons, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract the default favicon when there are no favicon in the
// message.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoFavicons) {
  base::ListValue favicons;

  std::vector<web::FaviconURL> urls;
  bool result =
      web::ExtractFaviconURL(favicons, GURL("http://chromium.org"), &urls);

  EXPECT_TRUE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://chromium.org/favicon.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());
}

// Tries to extract favicons with the sizes attributes containing one correct
// size and one incorrectly formatted.
TEST_F(FaviconUtilTest, ExtractFaviconURLSizesCorrectAndGarbage) {
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  favicon.Set("sizes", "10x20 sgxer");
  base::DictValue favicon2;
  favicon2.Set("href", "http://fav2.ico");
  favicon2.Set("rel", "apple-touch-icon");
  favicon2.Set("sizes", "sgxer 30x40");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(favicons, GURL(), &urls);

  EXPECT_TRUE(result);
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
  base::DictValue favicon;
  favicon.Set("href", "http://fav.ico");
  favicon.Set("rel", "icon");
  favicon.Set("sizes", "10x");
  base::DictValue favicon2;
  favicon2.Set("href", "http://fav2.ico");
  favicon2.Set("rel", "apple-touch-icon");
  favicon2.Set("sizes", "x40");
  base::ListValue favicons;
  favicons.Append(std::move(favicon));
  favicons.Append(std::move(favicon2));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(favicons, GURL(), &urls);

  EXPECT_TRUE(result);
  ASSERT_EQ(2U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());

  EXPECT_EQ(GURL("http://fav2.ico"), urls[1].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kTouchIcon, urls[1].icon_type);
  EXPECT_EQ(0U, urls[1].icon_sizes.size());
}

}  // namespace web

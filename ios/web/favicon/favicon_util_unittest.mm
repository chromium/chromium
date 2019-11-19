// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/favicon/favicon_util.h"

#include "base/values.h"
#include "ios/web/public/favicon/favicon_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using FaviconUtilTest = PlatformTest;

// Tries to extract multiple favicons url, all should be extracted.
TEST_F(FaviconUtilTest, ExtractFaviconURLMultipleFavicons) {
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  favicon.SetKey("sizes", base::Value("10x20"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("href", base::Value("http://fav2.ico"));
  favicon2.SetKey("rel", base::Value("apple-touch-icon"));
  favicon2.SetKey("sizes", base::Value("10x20 30x40"));
  base::DictionaryValue favicon3;
  favicon3.SetKey("href", base::Value("http://fav3.ico"));
  favicon3.SetKey("rel", base::Value("apple-touch-icon-precomposed"));
  favicon3.SetKey("sizes", base::Value("werfxw"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  favicons->Append(std::move(favicon3));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result =
      web::ExtractFaviconURL(&dictionnary, GURL("http://chromium.org"), &urls);

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
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("href", base::Value("http://fav2.ico"));
  base::DictionaryValue favicon3;
  favicon3.SetKey("href", base::Value("http://fav3.ico"));
  favicon3.SetKey("rel", base::Value("apple-touch-icon-precomposed"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  favicons->Append(std::move(favicon3));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(&dictionnary, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract favicons with the rel attributes being an int.
TEST_F(FaviconUtilTest, ExtractFaviconURLIntRel) {
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("href", base::Value("http://fav2.ico"));
  favicon2.SetKey("rel", base::Value(12));
  base::DictionaryValue favicon3;
  favicon3.SetKey("href", base::Value("http://fav3.ico"));
  favicon3.SetKey("rel", base::Value("apple-touch-icon-precomposed"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  favicons->Append(std::move(favicon3));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(&dictionnary, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract favicons with the href attributes missing in one of them.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoHref) {
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("rel", base::Value("apple-touch-icon"));
  base::DictionaryValue favicon3;
  favicon3.SetKey("href", base::Value("http://fav3.ico"));
  favicon3.SetKey("rel", base::Value("apple-touch-icon-precomposed"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  favicons->Append(std::move(favicon3));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(&dictionnary, GURL(), &urls);

  EXPECT_FALSE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://fav.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
}

// Tries to extract the default favicon when there are no favicon in the
// message.
TEST_F(FaviconUtilTest, ExtractFaviconURLNoFavicons) {
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result =
      web::ExtractFaviconURL(&dictionnary, GURL("http://chromium.org"), &urls);

  EXPECT_TRUE(result);
  ASSERT_EQ(1U, urls.size());
  EXPECT_EQ(GURL("http://chromium.org/favicon.ico"), urls[0].icon_url);
  EXPECT_EQ(web::FaviconURL::IconType::kFavicon, urls[0].icon_type);
  EXPECT_EQ(0U, urls[0].icon_sizes.size());
}

// Tries to extract favicons with the sizes attributes containing one correct
// size and one incorrectly formatted.
TEST_F(FaviconUtilTest, ExtractFaviconURLSizesCorrectAndGarbage) {
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  favicon.SetKey("sizes", base::Value("10x20 sgxer"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("href", base::Value("http://fav2.ico"));
  favicon2.SetKey("rel", base::Value("apple-touch-icon"));
  favicon2.SetKey("sizes", base::Value("sgxer 30x40"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(&dictionnary, GURL(), &urls);

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
  base::DictionaryValue favicon;
  favicon.SetKey("href", base::Value("http://fav.ico"));
  favicon.SetKey("rel", base::Value("icon"));
  favicon.SetKey("sizes", base::Value("10x"));
  base::DictionaryValue favicon2;
  favicon2.SetKey("href", base::Value("http://fav2.ico"));
  favicon2.SetKey("rel", base::Value("apple-touch-icon"));
  favicon2.SetKey("sizes", base::Value("x40"));
  std::unique_ptr<base::ListValue> favicons =
      std::make_unique<base::ListValue>();
  favicons->Append(std::move(favicon));
  favicons->Append(std::move(favicon2));
  base::DictionaryValue dictionnary;
  dictionnary.Set({"favicons"}, std::move(favicons));

  std::vector<web::FaviconURL> urls;
  bool result = web::ExtractFaviconURL(&dictionnary, GURL(), &urls);

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

// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#include <memory>
#include <vector>

#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/test/test_url_constants.h"
#include "net/base/escape.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace wk_navigation_util {

namespace {

// Creates a vector with given number of navigation items. All items will
// have distinct titles and URLs.
void CreateTestNavigationItems(
    size_t count,
    std::vector<std::unique_ptr<NavigationItem>>& items) {
  for (size_t i = 0; i < count; i++) {
    auto item = std::make_unique<NavigationItemImpl>();
    item->SetURL(GURL(base::StringPrintf("http://www.%zu.com", i)));
    item->SetTitle(base::ASCIIToUTF16(base::StringPrintf("Test%zu", i)));
    items.push_back(std::move(item));
  }
}

// Extracts session dictionary from |restore_session_url|.
base::JSONReader::ValueWithError ExtractSessionDict(GURL restore_session_url) {
  NSString* fragment = net::NSURLWithGURL(restore_session_url).fragment;
  NSString* encoded_session =
      [fragment substringFromIndex:strlen(kRestoreSessionSessionHashPrefix)];
  std::string session_json =
      net::UnescapeBinaryURLComponent(base::SysNSStringToUTF8(encoded_session));
  return base::JSONReader::ReadAndReturnValueWithError(session_json,
                                                       base::JSON_PARSE_RFC);
}

}  // namespace

typedef PlatformTest WKNavigationUtilTest;

TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrl) {
  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(GURL("http://www.0.com"));
  item0->SetTitle(base::ASCIIToUTF16("Test Website 0"));
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(GURL("http://www.1.com"));
  // Create an App-specific URL
  auto item2 = std::make_unique<NavigationItemImpl>();
  GURL url2("http://webui");
  GURL::Replacements scheme_replacements;
  scheme_replacements.SetSchemeStr(kTestWebUIScheme);
  item2->SetURL(url2.ReplaceComponents(scheme_replacements));

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));
  items.push_back(std::move(item2));

  int first_index = 0;
  GURL restore_session_url;
  CreateRestoreSessionUrl(0 /* last_committed_item_index */, items,
                          &restore_session_url, &first_index);
  ASSERT_EQ(0, first_index);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  std::string session_json =
      net::UnescapeBinaryURLComponent(restore_session_url.ref());

  EXPECT_EQ("session={\"offset\":-2,\"titles\":[\"Test Website 0\",\"\",\"\"],"
            "\"urls\":[\"http://www.0.com/\",\"http://www.1.com/\","
            "\"testwebui://webui/\"]}",
            session_json);
}

// In the past the math within CreateRestoreSessionUrl has had some edge case
// crashes.  Ensure that nothing crashes.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionBruteForce) {
  std::vector<std::unique_ptr<NavigationItem>> items;
  int first_index = 0;
  GURL restore_session_url;
  for (int num_items = 70; num_items < 80; num_items++) {
    std::vector<std::unique_ptr<NavigationItem>> items;
    CreateTestNavigationItems(num_items, items);
    for (int last_committed_index = 0; last_committed_index < num_items;
         last_committed_index++) {
      CreateRestoreSessionUrl(last_committed_index, items, &restore_session_url,
                              &first_index);
      // Extract session JSON from restoration URL.
      base::JSONReader::ValueWithError value_with_error =
          ExtractSessionDict(restore_session_url);

      base::Value* urls_value = value_with_error.value->FindKey("urls");
      if (num_items > kMaxSessionSize) {
        ASSERT_EQ(kMaxSessionSize,
                  static_cast<int>(urls_value->GetList().size()));
      } else {
        ASSERT_EQ(num_items, static_cast<int>(urls_value->GetList().size()));
      }
    }
  }
}

// Verifies that large session can be stored in NSURL. GURL is converted to
// NSURL, because NSURL is passed to WKWebView during the session restoration.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrlForLargeSession) {
  // Create restore session URL with large number of items.
  const size_t kItemCount = kMaxSessionSize;
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(kItemCount, items);
  int first_index = 0;
  GURL restore_session_url;
  CreateRestoreSessionUrl(
      /*last_committed_item_index=*/0, items, &restore_session_url,
      &first_index);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  // Extract session JSON from restoration URL.
  base::JSONReader::ValueWithError value_with_error =
      ExtractSessionDict(restore_session_url);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, value_with_error.error_code);
  ASSERT_TRUE(value_with_error.value.has_value());

  // Verify that all titles and URLs are present.
  base::Value* titles_value = value_with_error.value->FindKey("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_TRUE(titles_value->is_list());
  ASSERT_EQ(kItemCount, titles_value->GetList().size());

  base::Value* urls_value = value_with_error.value->FindKey("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_TRUE(urls_value->is_list());
  ASSERT_EQ(kItemCount, urls_value->GetList().size());
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the right side of |last_committed_item_index|.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrlForExtraLargeForwardList) {
  // Create restore session URL with large number of items that exceeds
  // kMaxSessionSize.
  const size_t kItemCount = kMaxSessionSize * 3;
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(kItemCount, items);
  int first_index = 0;
  GURL restore_session_url;
  CreateRestoreSessionUrl(
      /*last_committed_item_index=*/0, items, &restore_session_url,
      &first_index);
  ASSERT_EQ(0, first_index);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  // Extract session JSON from restoration URL.
  base::JSONReader::ValueWithError value_with_error =
      ExtractSessionDict(restore_session_url);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, value_with_error.error_code);
  ASSERT_TRUE(value_with_error.value.has_value());

  // Verify that first kMaxSessionSize titles and URLs are present.
  base::Value* titles_value = value_with_error.value->FindKey("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_TRUE(titles_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize),
            titles_value->GetList().size());
  ASSERT_EQ("Test0", titles_value->GetList()[0].GetString());
  ASSERT_EQ("Test74", titles_value->GetList()[kMaxSessionSize - 1].GetString());

  base::Value* urls_value = value_with_error.value->FindKey("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_TRUE(urls_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->GetList().size());
  ASSERT_EQ("http://www.0.com/", urls_value->GetList()[0].GetString());
  ASSERT_EQ("http://www.74.com/",
            urls_value->GetList()[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ(1 - kMaxSessionSize,
            value_with_error.value->FindKey("offset")->GetInt());
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the left side of |last_committed_item_index|.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrlForExtraLargeBackList) {
  // Create restore session URL with large number of items that exceeds
  // kMaxSessionSize.
  const size_t kItemCount = kMaxSessionSize * 3;
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(kItemCount, items);
  int first_index = 0;
  GURL restore_session_url;
  CreateRestoreSessionUrl(
      /*last_committed_item_index=*/kItemCount - 1, items, &restore_session_url,
      &first_index);
  ASSERT_EQ(150, first_index);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  // Extract session JSON from restoration URL.
  base::JSONReader::ValueWithError value_with_error =
      ExtractSessionDict(restore_session_url);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, value_with_error.error_code);
  ASSERT_TRUE(value_with_error.value.has_value());

  // Verify that last kMaxSessionSize titles and URLs are present.
  base::Value* titles_value = value_with_error.value->FindKey("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_TRUE(titles_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize),
            titles_value->GetList().size());
  ASSERT_EQ("Test150", titles_value->GetList()[0].GetString());
  ASSERT_EQ("Test224",
            titles_value->GetList()[kMaxSessionSize - 1].GetString());

  base::Value* urls_value = value_with_error.value->FindKey("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_TRUE(urls_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->GetList().size());
  ASSERT_EQ("http://www.150.com/", urls_value->GetList()[0].GetString());
  ASSERT_EQ("http://www.224.com/",
            urls_value->GetList()[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ(0, value_with_error.value->FindKey("offset")->GetInt());
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the left and right sides of |last_committed_item_index|.
TEST_F(WKNavigationUtilTest,
       CreateRestoreSessionUrlForExtraLargeBackAndForwardList) {
  // Create restore session URL with large number of items that exceeds
  // kMaxSessionSize.
  const size_t kItemCount = kMaxSessionSize * 2;
  std::vector<std::unique_ptr<NavigationItem>> items;
  CreateTestNavigationItems(kItemCount, items);
  int first_index = 0;
  GURL restore_session_url;
  CreateRestoreSessionUrl(
      /*last_committed_item_index=*/kMaxSessionSize, items,
      &restore_session_url, &first_index);
  ASSERT_EQ(38, first_index);
  ASSERT_TRUE(IsRestoreSessionUrl(restore_session_url));

  // Extract session JSON from restoration URL.
  base::JSONReader::ValueWithError value_with_error =
      ExtractSessionDict(restore_session_url);
  ASSERT_EQ(base::JSONReader::JSON_NO_ERROR, value_with_error.error_code);
  ASSERT_TRUE(value_with_error.value.has_value());

  // Verify that last kMaxSessionSize titles and URLs are present.
  base::Value* titles_value = value_with_error.value->FindKey("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_TRUE(titles_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize),
            titles_value->GetList().size());
  ASSERT_EQ("Test38", titles_value->GetList()[0].GetString());
  ASSERT_EQ("Test112",
            titles_value->GetList()[kMaxSessionSize - 1].GetString());

  base::Value* urls_value = value_with_error.value->FindKey("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_TRUE(urls_value->is_list());
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->GetList().size());
  ASSERT_EQ("http://www.38.com/", urls_value->GetList()[0].GetString());
  ASSERT_EQ("http://www.112.com/",
            urls_value->GetList()[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ((1 - kMaxSessionSize) / 2,
            value_with_error.value->FindKey("offset")->GetInt());
}

TEST_F(WKNavigationUtilTest, IsNotRestoreSessionUrl) {
  EXPECT_FALSE(IsRestoreSessionUrl(GURL()));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("file://somefile")));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("http://www.1.com")));
}

// Tests that CreateRedirectUrl and ExtractTargetURL used back-to-back is an
// identity transformation.
TEST_F(WKNavigationUtilTest, CreateAndExtractTargetURL) {
  GURL target_url = GURL("http://www.1.com?query=special%26chars");
  GURL url = CreateRedirectUrl(target_url);
  ASSERT_TRUE(url.SchemeIsFile());

  GURL extracted_url;
  ASSERT_TRUE(ExtractTargetURL(url, &extracted_url));
  EXPECT_EQ(target_url, extracted_url);
}

TEST_F(WKNavigationUtilTest, IsPlaceholderUrl) {
  // Valid placeholder URLs.
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=")));
  EXPECT_TRUE(IsPlaceholderUrl(GURL("about:blank?for=chrome%3A%2F%2Fnewtab")));

  // Not an about:blank URL.
  EXPECT_FALSE(IsPlaceholderUrl(GURL::EmptyGURL()));
  // Missing ?for= query parameter.
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank")));
  EXPECT_FALSE(IsPlaceholderUrl(GURL("about:blank?chrome:%3A%2F%2Fnewtab")));
}

TEST_F(WKNavigationUtilTest, EncodReturnsEmptyOnInvalidUrls) {
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL::EmptyGURL()));
  EXPECT_EQ(GURL::EmptyGURL(), CreatePlaceholderUrlForUrl(GURL("notaurl")));
}

TEST_F(WKNavigationUtilTest, EncodeDecodeValidUrls) {
  {
    GURL original("chrome://chrome-urls");
    GURL encoded("about:blank?for=chrome%3A%2F%2Fchrome-urls");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
  {
    GURL original("about:blank");
    GURL encoded("about:blank?for=about%3Ablank");
    EXPECT_EQ(encoded, CreatePlaceholderUrlForUrl(original));
    EXPECT_EQ(original, ExtractUrlFromPlaceholderUrl(encoded));
  }
}

// Tests that invalid URLs will be rejected in decoding.
TEST_F(WKNavigationUtilTest, DecodeRejectInvalidUrls) {
  GURL encoded("about:blank?for=thisisnotanurl");
  EXPECT_EQ(GURL::EmptyGURL(), ExtractUrlFromPlaceholderUrl(encoded));
}

// Tests that app specific urls and non-placeholder about: urls do not need a
// user agent type, but normal urls and placeholders do.
TEST_F(WKNavigationUtilTest, URLNeedsUserAgentType) {
  // Not app specific or non-placeholder about urls.
  GURL non_user_agent_urls("http://newtab");
  GURL::Replacements scheme_replacements;
  scheme_replacements.SetSchemeStr(kTestNativeContentScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));
  scheme_replacements.SetSchemeStr(url::kAboutScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));

  // Not a placeholder or normal URL.
  EXPECT_TRUE(URLNeedsUserAgentType(GURL("about:blank?for=")));
  EXPECT_TRUE(URLNeedsUserAgentType(GURL("http://www.0.com")));

  // file:// URL.
  EXPECT_FALSE(URLNeedsUserAgentType(GURL("file://foo.pdf")));

  // App specific URL or a placeholder for an app specific URL.
  GURL app_specific(
      url::SchemeHostPort(kTestAppSpecificScheme, "foo", 0).Serialize());
  EXPECT_FALSE(URLNeedsUserAgentType(app_specific));
  EXPECT_FALSE(URLNeedsUserAgentType(CreatePlaceholderUrlForUrl(app_specific)));
}

}  // namespace wk_navigation_util
}  // namespace web

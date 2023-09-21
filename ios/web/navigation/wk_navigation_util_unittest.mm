// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_navigation_util.h"

#import <memory>
#import <vector>

#import "base/json/json_reader.h"
#import "base/strings/escape.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/values.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/test/test_url_constants.h"
#import "net/base/mac/url_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/scheme_host_port.h"

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

// Extracts session dictionary from `restore_session_url`.
base::JSONReader::Result ExtractSessionDict(GURL restore_session_url) {
  NSString* fragment = net::NSURLWithGURL(restore_session_url).fragment;
  NSString* encoded_session =
      [fragment substringFromIndex:strlen(kRestoreSessionSessionHashPrefix)];
  std::string session_json = base::UnescapeBinaryURLComponent(
      base::SysNSStringToUTF8(encoded_session));
  return base::JSONReader::ReadAndReturnValueWithError(session_json,
                                                       base::JSON_PARSE_RFC);
}

}  // namespace

typedef PlatformTest WKNavigationUtilTest;

// Tests various inputs for GetSafeItemRange.
TEST_F(WKNavigationUtilTest, GetSafeItemRange) {
  // Session size does not exceed kMaxSessionSize and last_committed_item_index
  // is in range.
  for (int item_count = 0; item_count <= kMaxSessionSize; item_count++) {
    for (int item_index = 0; item_index < item_count; item_index++) {
      int offset = 0;
      int size = 0;
      EXPECT_EQ(item_index,
                GetSafeItemRange(item_index, item_count, &offset, &size))
          << "item_count: " << item_count << " item_index: " << item_index;
      EXPECT_EQ(0, offset) << "item_count: " << item_count
                           << " item_index: " << item_index;
      EXPECT_EQ(item_count, size)
          << "item_count: " << item_count << " item_index: " << item_index;
    }
  }

  // Session size is 1 item longer than kMaxSessionSize.
  int offset = 0;
  int size = 0;
  EXPECT_EQ(0, GetSafeItemRange(0, kMaxSessionSize + 1, &offset, &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(
      kMaxSessionSize - 1,
      GetSafeItemRange(kMaxSessionSize, kMaxSessionSize + 1, &offset, &size));
  EXPECT_EQ(1, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2,
            GetSafeItemRange(kMaxSessionSize / 2, kMaxSessionSize + 1, &offset,
                             &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2,
            GetSafeItemRange(kMaxSessionSize / 2 + 1, kMaxSessionSize + 1,
                             &offset, &size));
  EXPECT_EQ(1, offset);
  EXPECT_EQ(kMaxSessionSize, size);

  offset = 0;
  size = 0;
  EXPECT_EQ(kMaxSessionSize / 2 - 1,
            GetSafeItemRange(kMaxSessionSize / 2 - 1, kMaxSessionSize + 1,
                             &offset, &size));
  EXPECT_EQ(0, offset);
  EXPECT_EQ(kMaxSessionSize, size);
}

TEST_F(WKNavigationUtilTest, CreateRestoreSessionUrl) {
  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(GURL("http://www.0.com"));
  item0->SetTitle(u"Test Website 0");
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
  ASSERT_TRUE(IsRestoreSessionUrl(net::NSURLWithGURL(restore_session_url)));

  std::string session_json =
      base::UnescapeBinaryURLComponent(restore_session_url.ref());

  EXPECT_EQ("session={\"offset\":-2,\"titles\":[\"Test Website 0\",\"\",\"\"],"
            "\"urls\":[\"http://www.0.com/\",\"http://www.1.com/\","
            "\"testwebui://webui/\"]}",
            session_json);
}

// In the past the math within CreateRestoreSessionUrl has had some edge case
// crashes.  Ensure that nothing crashes.
TEST_F(WKNavigationUtilTest, CreateRestoreSessionBruteForce) {
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
      auto value_with_error = ExtractSessionDict(restore_session_url);
      const base::Value::Dict& dict = value_with_error->GetDict();

      const base::Value::List* urls_value = dict.FindList("urls");
      if (num_items > kMaxSessionSize) {
        ASSERT_EQ(kMaxSessionSize, static_cast<int>(urls_value->size()));
      } else {
        ASSERT_EQ(num_items, static_cast<int>(urls_value->size()));
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
  ASSERT_TRUE(IsRestoreSessionUrl(net::NSURLWithGURL(restore_session_url)));

  // Extract session JSON from restoration URL.
  auto value_with_error = ExtractSessionDict(restore_session_url);
  ASSERT_TRUE(value_with_error.has_value());
  const base::Value::Dict& dict = value_with_error->GetDict();

  // Verify that all titles and URLs are present.
  const base::Value::List* titles_value = dict.FindList("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_EQ(kItemCount, titles_value->size());

  const base::Value::List* urls_value = dict.FindList("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_EQ(kItemCount, urls_value->size());
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the right side of `last_committed_item_index`.
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
  ASSERT_TRUE(IsRestoreSessionUrl(net::NSURLWithGURL(restore_session_url)));

  // Extract session JSON from restoration URL.
  auto value_with_error = ExtractSessionDict(restore_session_url);
  ASSERT_TRUE(value_with_error.has_value());
  const base::Value::Dict& dict = value_with_error->GetDict();

  // Verify that first kMaxSessionSize titles and URLs are present.
  const base::Value::List* titles_value = dict.FindList("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), titles_value->size());
  ASSERT_EQ("Test0", (*titles_value)[0].GetString());
  ASSERT_EQ("Test74", (*titles_value)[kMaxSessionSize - 1].GetString());

  const base::Value::List* urls_value = dict.FindList("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->size());
  ASSERT_EQ("http://www.0.com/", (*urls_value)[0].GetString());
  ASSERT_EQ("http://www.74.com/",
            (*urls_value)[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ(1 - kMaxSessionSize, *dict.FindInt("offset"));
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the left side of `last_committed_item_index`.
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
  ASSERT_TRUE(IsRestoreSessionUrl(net::NSURLWithGURL(restore_session_url)));

  // Extract session JSON from restoration URL.
  auto value_with_error = ExtractSessionDict(restore_session_url);
  ASSERT_TRUE(value_with_error.has_value());
  const base::Value::Dict& dict = value_with_error->GetDict();

  // Verify that last kMaxSessionSize titles and URLs are present.
  const base::Value::List* titles_value = dict.FindList("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), titles_value->size());
  ASSERT_EQ("Test150", (*titles_value)[0].GetString());
  ASSERT_EQ("Test224", (*titles_value)[kMaxSessionSize - 1].GetString());

  const base::Value::List* urls_value = dict.FindList("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->size());
  ASSERT_EQ("http://www.150.com/", (*urls_value)[0].GetString());
  ASSERT_EQ("http://www.224.com/",
            (*urls_value)[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ(0, *dict.FindInt("offset"));
}

// Verifies that large session can be stored in NSURL and that extra items
// are trimmed from the left and right sides of `last_committed_item_index`.
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
  ASSERT_TRUE(IsRestoreSessionUrl(net::NSURLWithGURL(restore_session_url)));

  // Extract session JSON from restoration URL.
  auto value_with_error = ExtractSessionDict(restore_session_url);
  ASSERT_TRUE(value_with_error.has_value());
  const base::Value::Dict& dict = value_with_error->GetDict();

  // Verify that last kMaxSessionSize titles and URLs are present.
  const base::Value::List* titles_value = dict.FindList("titles");
  ASSERT_TRUE(titles_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), titles_value->size());
  ASSERT_EQ("Test38", (*titles_value)[0].GetString());
  ASSERT_EQ("Test112", (*titles_value)[kMaxSessionSize - 1].GetString());

  const base::Value::List* urls_value = dict.FindList("urls");
  ASSERT_TRUE(urls_value);
  ASSERT_EQ(static_cast<size_t>(kMaxSessionSize), urls_value->size());
  ASSERT_EQ("http://www.38.com/", (*urls_value)[0].GetString());
  ASSERT_EQ("http://www.112.com/",
            (*urls_value)[kMaxSessionSize - 1].GetString());

  // Verify the offset is correct.
  ASSERT_EQ((1 - kMaxSessionSize) / 2, *dict.FindInt("offset"));
}

TEST_F(WKNavigationUtilTest, IsNotRestoreSessionUrl) {
  EXPECT_FALSE(IsRestoreSessionUrl(GURL()));
  EXPECT_FALSE(IsRestoreSessionUrl([NSURL URLWithString:@""]));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("file://somefile")));
  EXPECT_FALSE(IsRestoreSessionUrl([NSURL URLWithString:@"file://somefile"]));
  EXPECT_FALSE(IsRestoreSessionUrl(GURL("http://www.1.com")));
  EXPECT_FALSE(IsRestoreSessionUrl([NSURL URLWithString:@"http://www.1.com"]));
}

// Tests that app specific urls and non-placeholder about: urls do not need a
// user agent type, but normal urls and placeholders do.
TEST_F(WKNavigationUtilTest, URLNeedsUserAgentType) {
  // Not app specific or non-placeholder about urls.
  GURL non_user_agent_urls("http://newtab");
  GURL::Replacements scheme_replacements;
  scheme_replacements.SetSchemeStr(kTestAppSpecificScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));
  scheme_replacements.SetSchemeStr(url::kAboutScheme);
  EXPECT_FALSE(URLNeedsUserAgentType(
      non_user_agent_urls.ReplaceComponents(scheme_replacements)));

  // about:blank pages.
  EXPECT_FALSE(URLNeedsUserAgentType(GURL("about:blank")));
  // Normal URL.
  EXPECT_TRUE(URLNeedsUserAgentType(GURL("http://www.0.com")));

  // file:// URL.
  EXPECT_FALSE(URLNeedsUserAgentType(GURL("file://foo.pdf")));

  // App specific URL or a placeholder for an app specific URL.
  GURL app_specific(
      url::SchemeHostPort(kTestAppSpecificScheme, "foo", 0).Serialize());
  EXPECT_FALSE(URLNeedsUserAgentType(app_specific));
}

}  // namespace wk_navigation_util
}  // namespace web

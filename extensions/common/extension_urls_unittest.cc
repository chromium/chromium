// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extension_urls {

// The tests in this file hard-code the expected webstore URLs. These are a bit
// of a change-detector test in some ways, but are valuable because:
// a) The webstore URLs *shouldn't* change often, and updating these tests if
//    they do is very cheap.
// b) The construction of the URLs is a bit subtle and we've had tricky bugs
//    in the past (e.g., double slashes or improperly resolved paths). These
//    ensure the construction succeeds properly with the default URL.

// Tests that the URL for the extensions category in the webstore is what
// we expect.
TEST(ExtensionWebstoreURLsTest, GetWebstoreExtensionsCategoryURL) {
  EXPECT_EQ("https://chromewebstore.google.com/category/extensions",
            GetWebstoreExtensionsCategoryURL().spec());
}

// Tests that the URL to get the block status of extensions in the webstore is
// what we expect.
TEST(ExtensionWebstoreURLsTest, GetWebstoreBlockStatusURL) {
  EXPECT_EQ(
      "https://chromewebstore.googleapis.com/v2/items:"
      "batchFetchItemBlockStatusForEnterprise",
      GetWebstoreBlockStatusURL().spec());
}

}  // namespace extension_urls

namespace extensions {
namespace {

GURL ExtensionURLWithPath(std::string_view path) {
  return GURL(base::StrCat(
      {"chrome-extension://abcdefghijklmnopabcdefghijklmnop", path}));
}

}  // namespace

// Tests that IsGeneratedFaviconURL() matches only the generated favicon
// endpoint.
TEST(ExtensionURLsTest, IsGeneratedFaviconURL) {
  EXPECT_TRUE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_favicon")));
  EXPECT_TRUE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_favicon/")));
  EXPECT_TRUE(IsGeneratedFaviconURL(
      ExtensionURLWithPath("/_favicon?pageUrl=https://example.com")));
  EXPECT_TRUE(IsGeneratedFaviconURL(
      ExtensionURLWithPath("/_favicon/?pageUrl=https://example.com")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_favicon//")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_favicon/foo")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_faviconfoo")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/_Favicon")));
  EXPECT_FALSE(IsGeneratedFaviconURL(ExtensionURLWithPath("/dir/_favicon")));
}

// Tests that IsGeneratedBackgroundPageURL() matches only the generated
// background page.
TEST(ExtensionURLsTest, IsGeneratedBackgroundPageURL) {
  EXPECT_TRUE(IsGeneratedBackgroundPageURL(
      ExtensionURLWithPath("/_generated_background_page.html")));
  EXPECT_TRUE(IsGeneratedBackgroundPageURL(
      ExtensionURLWithPath("/_generated_background_page.html?query")));
  EXPECT_FALSE(IsGeneratedBackgroundPageURL(ExtensionURLWithPath("/")));
  EXPECT_FALSE(
      IsGeneratedBackgroundPageURL(ExtensionURLWithPath("/background.html")));
  EXPECT_FALSE(IsGeneratedBackgroundPageURL(
      ExtensionURLWithPath("/_generated_background_page.html/")));
  EXPECT_FALSE(IsGeneratedBackgroundPageURL(
      ExtensionURLWithPath("/dir/_generated_background_page.html")));
}

}  // namespace extensions

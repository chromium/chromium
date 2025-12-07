// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "testing/gtest/include/gtest/gtest.h"

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

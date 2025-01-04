// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extension_urls {

// Tests that the URL for the extensions category in the webstore is what
// we expect.
TEST(ExtensionWebstoreURLsTest, GetWebstoreExtensionsCategoryURL) {
  // Hard-code the expected result. This is a bit of a change-detector
  // test, but is valuable because
  // a) The webstore URL *shouldn't* change often, and updating this test
  //    if it does is very cheap.
  // b) The construction of the URL in GetWebstoreExtensionsCategoryURL()
  //    is a bit subtle and we've had tricky bugs in the past (e.g.,
  //    double slashes or improperly resolved paths). This ensures the
  //    construction succeeds properly with the default URL.
  EXPECT_EQ("https://chromewebstore.google.com/category/extensions",
            GetWebstoreExtensionsCategoryURL().spec());
}

}  // namespace extension_urls

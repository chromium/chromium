// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "base/test/scoped_feature_list.h"
#include "extensions/common/extension_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extension_urls {

namespace {

class ExtensionWebstoreURLsTest : public testing::Test,
                                  public testing::WithParamInterface<bool> {
 public:
  explicit ExtensionWebstoreURLsTest(bool enable_new_webstore_url = false) {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          extensions_features::kNewWebstoreURL);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          extensions_features::kNewWebstoreURL);
    }
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(NewChromeWebstoreLaunchUrl,
                         ExtensionWebstoreURLsTest,
                         testing::Values(true));
INSTANTIATE_TEST_SUITE_P(PreviousChromeWebstoreLaunchUrl,
                         ExtensionWebstoreURLsTest,
                         testing::Values(false));

}  // namespace

// Tests that the correct extensions webstore category URL is returned depending
// on feature extensions_features::kNewWebstoreURL.
TEST_P(ExtensionWebstoreURLsTest, GetNewWebstoreExtensionsCategoryURL) {
  const std::string expected_category_url =
      GetParam() ? GetNewWebstoreLaunchURL().spec() + "category/extensions"
                 : GetWebstoreLaunchURL().spec() + "/category/extensions";
  EXPECT_EQ(expected_category_url, GetWebstoreExtensionsCategoryURL());
}

}  // namespace extension_urls

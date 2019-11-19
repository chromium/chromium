// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace bundled_exchanges_utils {

TEST(BundledExchangesUtilsTest, GetSynthesizedUrlForBundledExchanges) {
  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html"),
            GetSynthesizedUrlForBundledExchanges(
                GURL("file:///dir/x.wbn"), GURL("https://example.com/a.html")));

  EXPECT_EQ(
      GURL("file:///dir/x.wbn?https://example.com/a.html?query"),
      GetSynthesizedUrlForBundledExchanges(
          GURL("file:///dir/x.wbn"), GURL("https://example.com/a.html?query")));

  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html?query2"),
            GetSynthesizedUrlForBundledExchanges(
                GURL("file:///dir/x.wbn?query1"),
                GURL("https://example.com/a.html?query2")));

  EXPECT_EQ(
      GURL("file:///dir/x.wbn?https://example.com/a.html"),
      GetSynthesizedUrlForBundledExchanges(GURL("file:///dir/x.wbn#ref"),
                                           GURL("https://example.com/a.html")));

  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html#ref2"),
            GetSynthesizedUrlForBundledExchanges(
                GURL("file:///dir/x.wbn#ref1"),
                GURL("https://example.com/a.html#ref2")));

  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html?query2#ref2"),
            GetSynthesizedUrlForBundledExchanges(
                GURL("file:///dir/x.wbn?query1#ref1"),
                GURL("https://example.com/a.html?query2#ref2")));
}

}  // namespace bundled_exchanges_utils
}  // namespace content

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace web_bundle_utils {

TEST(WebBundleUtilsTest, GetSynthesizedUrlForWebBundle) {
  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html"),
            GetSynthesizedUrlForWebBundle(GURL("file:///dir/x.wbn"),
                                          GURL("https://example.com/a.html")));

  EXPECT_EQ(
      GURL("file:///dir/x.wbn?https://example.com/a.html?query"),
      GetSynthesizedUrlForWebBundle(GURL("file:///dir/x.wbn"),
                                    GURL("https://example.com/a.html?query")));

  EXPECT_EQ(
      GURL("file:///dir/x.wbn?https://example.com/a.html?query2"),
      GetSynthesizedUrlForWebBundle(GURL("file:///dir/x.wbn?query1"),
                                    GURL("https://example.com/a.html?query2")));

  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html"),
            GetSynthesizedUrlForWebBundle(GURL("file:///dir/x.wbn#ref"),
                                          GURL("https://example.com/a.html")));

  EXPECT_EQ(
      GURL("file:///dir/x.wbn?https://example.com/a.html#ref2"),
      GetSynthesizedUrlForWebBundle(GURL("file:///dir/x.wbn#ref1"),
                                    GURL("https://example.com/a.html#ref2")));

  EXPECT_EQ(GURL("file:///dir/x.wbn?https://example.com/a.html?query2#ref2"),
            GetSynthesizedUrlForWebBundle(
                GURL("file:///dir/x.wbn?query1#ref1"),
                GURL("https://example.com/a.html?query2#ref2")));
}

}  // namespace web_bundle_utils
}  // namespace content

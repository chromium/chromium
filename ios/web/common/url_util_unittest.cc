// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/url_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

using URLUtilTest = PlatformTest;

TEST_F(URLUtilTest, GURLByRemovingRefFromGURL) {
  GURL url("http://foo.com/bar#baz");
  EXPECT_EQ(GURL("http://foo.com/bar"), GURLByRemovingRefFromGURL(url));
}

}  // namespace web

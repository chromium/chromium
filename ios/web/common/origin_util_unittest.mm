// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/common/origin_util.h"

#import <WebKit/WebKit.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using OriginUtilTest = PlatformTest;

// Tests calling IsSecureOrigin with secure origins.
TEST_F(OriginUtilTest, IsSecureOriginSecure) {
  EXPECT_TRUE(IsOriginSecure(GURL("http://localhost")));
  EXPECT_TRUE(IsOriginSecure(GURL("https://chromium.org")));
  EXPECT_TRUE(IsOriginSecure(GURL("file://file")));
}

// Tests calling IsSecureOrigin with insecure origins.
TEST_F(OriginUtilTest, IsInsecureOriginSecure) {
  EXPECT_FALSE(IsOriginSecure(GURL("http://chromium.org")));
  EXPECT_FALSE(IsOriginSecure(GURL("bogus://bogus")));
}

}  // namespace web

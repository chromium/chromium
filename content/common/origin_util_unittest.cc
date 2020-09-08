// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/origin_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(OriginUtilTest, IsPotentiallyTrustworthyOrigin) {
  EXPECT_FALSE(
      IsPotentiallyTrustworthyOrigin(url::Origin::Create(GURL("about:blank"))));
  EXPECT_FALSE(IsPotentiallyTrustworthyOrigin(
      url::Origin::Create(GURL("about:blank#ref"))));
  EXPECT_FALSE(IsPotentiallyTrustworthyOrigin(
      url::Origin::Create(GURL("about:srcdoc"))));

  EXPECT_FALSE(IsPotentiallyTrustworthyOrigin(
      url::Origin::Create(GURL("javascript:alert('blah')"))));

  EXPECT_FALSE(IsPotentiallyTrustworthyOrigin(
      url::Origin::Create(GURL("data:test/plain;blah"))));
}

}  // namespace content

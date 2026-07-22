// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/content_type_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace web {

class ContentTypeUtilTest : public PlatformTest {};

TEST_F(ContentTypeUtilTest, TestIsContentTypeHtml) {
  EXPECT_TRUE(IsContentTypeHtml("text/html"));
  EXPECT_TRUE(IsContentTypeHtml("application/xhtml+xml"));
  EXPECT_TRUE(IsContentTypeHtml("application/xml"));

  EXPECT_FALSE(IsContentTypeHtml("text/xhtml"));
  EXPECT_FALSE(IsContentTypeHtml("application"));
  EXPECT_FALSE(IsContentTypeHtml("application/html"));
  EXPECT_FALSE(IsContentTypeHtml("application/xhtml"));
}

TEST_F(ContentTypeUtilTest, TestIsContentTypeImage) {
  EXPECT_TRUE(IsContentTypeImage("image/bmp"));
  EXPECT_TRUE(IsContentTypeImage("image/gif"));
  EXPECT_TRUE(IsContentTypeImage("image/png"));
  EXPECT_TRUE(IsContentTypeImage("image/x-icon"));

  EXPECT_FALSE(IsContentTypeImage("imag"));
  EXPECT_FALSE(IsContentTypeImage("text/html"));
}

TEST_F(ContentTypeUtilTest, TestIsContentTypePdf) {
  EXPECT_TRUE(IsContentTypePdf("application/pdf"));
  EXPECT_TRUE(IsContentTypePdf("APPLICATION/PDF"));
  EXPECT_TRUE(IsContentTypePdf("application/PDF"));

  EXPECT_FALSE(IsContentTypePdf("application/x-pdf"));
  EXPECT_FALSE(IsContentTypePdf("text/html"));
}

}  // namespace web

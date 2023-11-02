// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_status_code.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HttpStatusCode, OK) {
  EXPECT_EQ(200, HTTP_OK);
  EXPECT_STREQ("OK", GetHttpReasonPhrase(HTTP_OK));
}

}  // namespace

}  // namespace net

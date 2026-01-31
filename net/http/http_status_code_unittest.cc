// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_status_code.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HttpStatusCode, GetHttpReasonPhrase) {
  EXPECT_EQ("OK", GetHttpReasonPhrase(HTTP_OK));
  EXPECT_EQ("OK", GetHttpReasonPhrase(HTTP_OK, "Overridden Default"));
  EXPECT_EQ("OK", GetHttpReasonPhrase(200));
  EXPECT_EQ("OK", GetHttpReasonPhrase(200, "Overridden Default"));

  EXPECT_EQ("Not Found", GetHttpReasonPhrase(HTTP_NOT_FOUND));
  EXPECT_EQ("Not Found",
            GetHttpReasonPhrase(HTTP_NOT_FOUND, "Overridden Default"));
  EXPECT_EQ("Not Found", GetHttpReasonPhrase(404));
  EXPECT_EQ("Not Found", GetHttpReasonPhrase(404, "Overridden Default"));

  EXPECT_EQ("Unknown Status Code",
            GetHttpReasonPhrase(static_cast<HttpStatusCode>(599)));
  EXPECT_EQ("Overridden Default",
            GetHttpReasonPhrase(static_cast<HttpStatusCode>(599),
                                "Overridden Default"));
  EXPECT_EQ("Unknown Status Code", GetHttpReasonPhrase(599));
  EXPECT_EQ("Overridden Default",
            GetHttpReasonPhrase(599, "Overridden Default"));

  EXPECT_EQ("Unknown Status Code",
            GetHttpReasonPhrase(static_cast<HttpStatusCode>(1)));
  EXPECT_EQ("Unknown Status Code", GetHttpReasonPhrase(1));

  EXPECT_EQ("Unknown Status Code",
            GetHttpReasonPhrase(static_cast<HttpStatusCode>(12345)));
  EXPECT_EQ("Unknown Status Code", GetHttpReasonPhrase(12345));

  EXPECT_EQ("Unknown Status Code",
            GetHttpReasonPhrase(static_cast<HttpStatusCode>(-1)));
  EXPECT_EQ("Unknown Status Code", GetHttpReasonPhrase(-1));
}

}  // namespace

}  // namespace net

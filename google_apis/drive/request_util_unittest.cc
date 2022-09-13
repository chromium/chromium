// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/request_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {
namespace util {

TEST(GenerateIfMatchHeaderTest, GenerateIfMatchHeader) {
  // The header matched to all etag should be returned for empty etag.
  EXPECT_EQ("If-Match: *", GenerateIfMatchHeader(std::string()));

  // Otherwise, the returned header should be matched to the given etag.
  EXPECT_EQ("If-Match: abcde", GenerateIfMatchHeader("abcde"));
  EXPECT_EQ("If-Match: fake_etag", GenerateIfMatchHeader("fake_etag"));
}

}  // namespace util
}  // namespace google_apis

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/no_vary_search_header_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(NoVarySearchHeaderParserTest, NoVarySearchHasBooleanParamsMember) {
  EXPECT_TRUE(NoVarySearchHasBooleanParamsMember("params"));
  EXPECT_TRUE(NoVarySearchHasBooleanParamsMember("params=?1"));
  EXPECT_TRUE(NoVarySearchHasBooleanParamsMember("params=?0"));
  EXPECT_FALSE(NoVarySearchHasBooleanParamsMember("params=(\"a\")"));
  EXPECT_FALSE(NoVarySearchHasBooleanParamsMember("key-order"));
  EXPECT_FALSE(NoVarySearchHasBooleanParamsMember(""));
}

}  // namespace
}  // namespace network

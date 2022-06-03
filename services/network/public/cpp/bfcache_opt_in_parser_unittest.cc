// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/bfcache_opt_in_parser.h"

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(BFCacheOptInParser, ParseBFCacheOptInUnload) {
  const struct TestCase {
    const char* header_value;
    bool expected;
  } test_cases[] = {
      {"unload", true},
      {"", false},
      {"abcd", false},
      {"unload=no_param_expected", false},
      {"unknown, unload, unknown2", true},
      {"unload; ignored_key=\"value\"", true},
      {"(unload no-inner-list)", false},
      {"\"unload\"", false},
  };
  for (const auto& test_case : test_cases) {
    testing::Message scoped;
    scoped << "test_case.header_value: " << test_case.header_value;
    SCOPED_TRACE(scoped);

    EXPECT_EQ(test_case.expected,
              ParseBFCacheOptInUnload(test_case.header_value));
  }
}

}  // namespace
}  // namespace network

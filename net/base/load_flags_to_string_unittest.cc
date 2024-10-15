// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_flags_to_string.h"

#include <string>

#include "net/base/load_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(LoadFlagsToStringTest, Normal) {
  EXPECT_EQ(LoadFlagsToString(LOAD_NORMAL), "LOAD_NORMAL");
}

TEST(LoadFlagsToStringTest, OneFlag) {
  EXPECT_EQ(LoadFlagsToString(LOAD_DISABLE_CACHE), "LOAD_DISABLE_CACHE");
}

TEST(LoadFlagsToStringTest, TwoFlags) {
  EXPECT_EQ(LoadFlagsToString(LOAD_DO_NOT_SAVE_COOKIES | LOAD_PREFETCH),
            "LOAD_DO_NOT_SAVE_COOKIES | LOAD_PREFETCH");
}

TEST(LoadFlagsToStringTest, ThreeFlags) {
  EXPECT_EQ(
      LoadFlagsToString(LOAD_BYPASS_CACHE | LOAD_CAN_USE_SHARED_DICTIONARY |
                        LOAD_SHOULD_BYPASS_HSTS),
      "LOAD_BYPASS_CACHE | LOAD_CAN_USE_SHARED_DICTIONARY | "
      "LOAD_SHOULD_BYPASS_HSTS");
}

}  // namespace net

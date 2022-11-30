// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

TEST(StringUtilTest, IsValidObjectPath) {
  EXPECT_TRUE(IsValidObjectPath("/"));
  EXPECT_TRUE(IsValidObjectPath("/foo/bar"));
  EXPECT_TRUE(IsValidObjectPath("/hoge_fuga/piyo123"));
  // Empty string.
  EXPECT_FALSE(IsValidObjectPath(std::string()));
  // Empty element.
  EXPECT_FALSE(IsValidObjectPath("//"));
  EXPECT_FALSE(IsValidObjectPath("/foo//bar"));
  EXPECT_FALSE(IsValidObjectPath("/foo///bar"));
  // Trailing '/'.
  EXPECT_FALSE(IsValidObjectPath("/foo/"));
  EXPECT_FALSE(IsValidObjectPath("/foo/bar/"));
  // Not beginning with '/'.
  EXPECT_FALSE(IsValidObjectPath("foo/bar"));
  // Invalid characters.
  EXPECT_FALSE(IsValidObjectPath("/foo.bar"));
  EXPECT_FALSE(IsValidObjectPath("/foo/*"));
  EXPECT_FALSE(IsValidObjectPath("/foo/bar(1)"));
}

}  // namespace dbus

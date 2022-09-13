// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {

TEST(UtilTest, GetAbsoluteMemberName) {
  EXPECT_EQ("InterfaceName.MemberName",
            GetAbsoluteMemberName("InterfaceName", "MemberName"));
  EXPECT_EQ(".", GetAbsoluteMemberName("", ""));
}

}  // namespace dbus

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/port_util.h"

#include <string>

#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  const std::vector<uint16_t> valid[] = {
      {}, {1}, {1, 2}, {1, 2, 3}, {10, 11, 12, 13}};

  for (size_t i = 0; i < base::size(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

}  // namespace net

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/winhttp/net_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace winhttp {

TEST(WinHttpUtil, HRESULTFromLastError) {
  ::SetLastError(ERROR_ACCESS_DENIED);
  EXPECT_EQ(E_ACCESSDENIED, HRESULTFromLastError());
  ::SetLastError(ERROR_SUCCESS);
  EXPECT_EQ(E_FAIL, HRESULTFromLastError());
}

}  // namespace winhttp

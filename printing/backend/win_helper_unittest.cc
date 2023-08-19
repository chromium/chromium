// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/win_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(WinHelperTest, GetDriverVersionString) {
  // A sample driver version, as represented by DRIVER_INFO_6.dwlDriverVersion.
  // https://learn.microsoft.com/en-us/windows/win32/printdocs/driver-info-6
  constexpr DWORDLONG kDriverVersion = 0x1234567813572468;

  // Breaking the version number into 16 bit chunks:
  //   0x1234.0x5678.0x1357.0x2468
  // which after conversion to decimal becomes:
  //   4660.22136.4951.9320
  EXPECT_EQ(GetDriverVersionStringForTesting(kDriverVersion),
            "4660.22136.4951.9320");
}

}  // namespace printing

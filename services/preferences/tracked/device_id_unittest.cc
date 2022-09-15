// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/device_id.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GetDeterministicMachineSpecificIdTest, IsDeterministic) {
  std::string first_machine_id;
  std::string second_machine_id;

  const MachineIdStatus kExpectedStatus =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      MachineIdStatus::SUCCESS;
#else
      MachineIdStatus::NOT_IMPLEMENTED;
#endif

  ASSERT_EQ(kExpectedStatus,
            GetDeterministicMachineSpecificId(&first_machine_id));
  ASSERT_EQ(kExpectedStatus,
            GetDeterministicMachineSpecificId(&second_machine_id));

  // The reason for using |EXPECT_TRUE| with one argument instead of |EXPECT_EQ|
  // with two arguments is a compiler bug in gcc that results in a "converting
  // 'false' to pointer type" error when the first argument to |EXPECT_EQ| is a
  // compile-time const false value. See also the following bug reports:
  // https://code.google.com/p/googletest/issues/detail?id=322
  // https://code.google.com/p/googletest/issues/detail?id=458
  EXPECT_TRUE((kExpectedStatus == MachineIdStatus::SUCCESS) ==
              !first_machine_id.empty());
  EXPECT_EQ(first_machine_id, second_machine_id);
}

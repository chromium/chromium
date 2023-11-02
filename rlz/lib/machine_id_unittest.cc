// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/machine_id.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "rlz/test/rlz_test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

// This test will fail if the behavior of GetMachineId changes.
TEST(MachineDealCodeTestMachineId, MachineId) {
  std::u16string computer_sid(u"S-1-5-21-2345599882-2448789067-1921365677");
  std::string id;
  rlz_lib::testing::GetMachineIdImpl(computer_sid, -1643738288, &id);
  EXPECT_STREQ("A341BA986A7E86840688977FCF20C86E253F00919E068B50F8",
               id.c_str());
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(MachineDealCodeTestMachineId, MachineIdIsUnique) {
  std::string id1;
  std::string id2;
  rlz_lib::GetMachineId(&id1);
  rlz_lib::GetMachineId(&id2);
  EXPECT_NE(id1, id2);
}

TEST(MachineDealCodeTestMachineId, MachineIdIsProperFormat) {
  std::string id;
  rlz_lib::GetMachineId(&id);
  std::string prefix = id.substr(0, 5);

  EXPECT_EQ(50u, id.length());
  EXPECT_TRUE(
      base::ContainsOnlyChars(id, "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"));
  EXPECT_STREQ("NONCE", prefix.c_str());
}
#endif

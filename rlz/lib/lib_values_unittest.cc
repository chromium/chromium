// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/lib_values.h"

#include <optional>

#include "rlz/lib/assert.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LibValuesUnittest, GetAccessPointFromName) {
  EXPECT_EQ(rlz_lib::GetAccessPointFromName(""), rlz_lib::NO_ACCESS_POINT);
  EXPECT_EQ(rlz_lib::GetAccessPointFromName("i1"), std::nullopt);
  EXPECT_EQ(rlz_lib::GetAccessPointFromName("I7"), rlz_lib::IE_DEFAULT_SEARCH);
  EXPECT_EQ(rlz_lib::GetAccessPointFromName("T4"), rlz_lib::IETB_SEARCH_BOX);
  EXPECT_EQ(rlz_lib::GetAccessPointFromName("T4 "), std::nullopt);

  for (int ap = rlz_lib::NO_ACCESS_POINT + 1;
       ap < rlz_lib::LAST_ACCESS_POINT; ++ap) {
    EXPECT_FALSE(
        GetAccessPointName(static_cast<rlz_lib::AccessPoint>(ap)).empty());
  }
}

TEST(LibValuesUnittest, GetEventFromName) {
  EXPECT_EQ(rlz_lib::GetEventFromName(""), rlz_lib::INVALID_EVENT);
  EXPECT_EQ(rlz_lib::GetEventFromName("i1"), std::nullopt);
  EXPECT_EQ(rlz_lib::GetEventFromName("I"), rlz_lib::INSTALL);
  EXPECT_EQ(rlz_lib::GetEventFromName("F"), rlz_lib::FIRST_SEARCH);
  EXPECT_EQ(rlz_lib::GetEventFromName("F "), std::nullopt);
  EXPECT_EQ(rlz_lib::GetEventFromName("X"), rlz_lib::ENTERPRISE_ENROLLMENT);
  EXPECT_EQ(rlz_lib::GetEventFromName("Y"), rlz_lib::ENTERPRISE_UNENROLLMENT);
  EXPECT_EQ(rlz_lib::GetEventFromName("Z"),
            rlz_lib::ENTERPRISE_ENROLLED_ACTIVATE);
  EXPECT_EQ(rlz_lib::GetEventFromName("W"),
            rlz_lib::ENTERPRISE_ENROLLED_FIRST_SEARCH);
}

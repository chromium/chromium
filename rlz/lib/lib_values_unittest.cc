// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/lib_values.h"

#include "rlz/lib/assert.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LibValuesUnittest, GetAccessPointFromName) {
  rlz_lib::SetExpectedAssertion("GetAccessPointFromName: point is NULL");
  EXPECT_FALSE(rlz_lib::GetAccessPointFromName("", NULL));
  rlz_lib::SetExpectedAssertion("");

  rlz_lib::AccessPoint point;
  EXPECT_FALSE(rlz_lib::GetAccessPointFromName(NULL, &point));
  EXPECT_EQ(rlz_lib::NO_ACCESS_POINT, point);

  EXPECT_TRUE(rlz_lib::GetAccessPointFromName("", &point));
  EXPECT_EQ(rlz_lib::NO_ACCESS_POINT, point);

  EXPECT_FALSE(rlz_lib::GetAccessPointFromName("i1", &point));
  EXPECT_EQ(rlz_lib::NO_ACCESS_POINT, point);

  EXPECT_TRUE(rlz_lib::GetAccessPointFromName("I7", &point));
  EXPECT_EQ(rlz_lib::IE_DEFAULT_SEARCH, point);

  EXPECT_TRUE(rlz_lib::GetAccessPointFromName("T4", &point));
  EXPECT_EQ(rlz_lib::IETB_SEARCH_BOX, point);

  EXPECT_FALSE(rlz_lib::GetAccessPointFromName("T4 ", &point));
  EXPECT_EQ(rlz_lib::NO_ACCESS_POINT, point);

  for (int ap = rlz_lib::NO_ACCESS_POINT + 1;
       ap < rlz_lib::LAST_ACCESS_POINT; ++ap) {
    EXPECT_TRUE(GetAccessPointName(static_cast<rlz_lib::AccessPoint>(ap)) !=
                NULL);
  }
}

TEST(LibValuesUnittest, GetEventFromName) {
  rlz_lib::SetExpectedAssertion("GetEventFromName: event is NULL");
  EXPECT_FALSE(rlz_lib::GetEventFromName("", NULL));
  rlz_lib::SetExpectedAssertion("");

  rlz_lib::Event event;
  EXPECT_FALSE(rlz_lib::GetEventFromName(NULL, &event));
  EXPECT_EQ(rlz_lib::INVALID_EVENT, event);

  EXPECT_TRUE(rlz_lib::GetEventFromName("", &event));
  EXPECT_EQ(rlz_lib::INVALID_EVENT, event);

  EXPECT_FALSE(rlz_lib::GetEventFromName("i1", &event));
  EXPECT_EQ(rlz_lib::INVALID_EVENT, event);

  EXPECT_TRUE(rlz_lib::GetEventFromName("I", &event));
  EXPECT_EQ(rlz_lib::INSTALL, event);

  EXPECT_TRUE(rlz_lib::GetEventFromName("F", &event));
  EXPECT_EQ(rlz_lib::FIRST_SEARCH, event);

  EXPECT_FALSE(rlz_lib::GetEventFromName("F ", &event));
  EXPECT_EQ(rlz_lib::INVALID_EVENT, event);
}

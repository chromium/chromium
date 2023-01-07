// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_event_log_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::protocol {

namespace {

// Converts a log section to a std::string for convenient testing.
std::string ToString(const WebrtcEventLogData::LogSection& section) {
  return std::string(section.begin(), section.end());
}

}  // namespace

TEST(WebrtcEventLogDataTest, WriteSingleEntry_IsStored) {
  WebrtcEventLogData event_log;
  event_log.Write("abc");
  auto data = event_log.TakeLogData();

  ASSERT_EQ(data.size(), 1U);
  EXPECT_EQ(ToString(data[0]), "abc");
}

TEST(WebrtcEventLogDataTest, WriteBeyondSection_OverflowsToNewSection) {
  WebrtcEventLogData event_log;
  event_log.SetMaxSectionSizeForTest(5);
  event_log.Write("aa");
  event_log.Write("bb");
  event_log.Write("cc");
  event_log.Write("dd");
  event_log.Write("ee");
  auto data = event_log.TakeLogData();

  ASSERT_EQ(data.size(), 3U);
  EXPECT_EQ(ToString(data[0]), "aabb");
  EXPECT_EQ(ToString(data[1]), "ccdd");
  EXPECT_EQ(ToString(data[2]), "ee");
}

TEST(WebrtcEventLogDataTest, WriteTooMuchData_OldestSectionDeleted) {
  WebrtcEventLogData event_log;
  event_log.SetMaxSectionSizeForTest(5);
  event_log.SetMaxSectionsForTest(2);
  event_log.Write("aa");
  event_log.Write("bb");
  event_log.Write("cc");
  event_log.Write("dd");
  event_log.Write("ee");
  auto data = event_log.TakeLogData();

  ASSERT_EQ(data.size(), 2U);
  EXPECT_EQ(ToString(data[0]), "ccdd");
  EXPECT_EQ(ToString(data[1]), "ee");
}

TEST(WebrtcEventLogDataTest, StoreThenClear_IsEmpty) {
  WebrtcEventLogData event_log;
  event_log.Write("aaa");
  event_log.Clear();
  auto data = event_log.TakeLogData();

  EXPECT_TRUE(data.empty());
}

}  // namespace remoting::protocol

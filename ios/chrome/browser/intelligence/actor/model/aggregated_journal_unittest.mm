// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/model/aggregated_journal.h"

#import <vector>

#import "base/strings/string_number_conversions.h"
#import "base/test/task_environment.h"
#import "base/token.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace actor {

class AggregatedJournalTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(AggregatedJournalTest, LogInstant) {
  AggregatedJournal journal;
  ActorTaskId task_id(1);
  journal.Log(GURL("https://example.com"), task_id, "TestEvent",
              {{"key", "value"}});

  auto logs = journal.GetLogs();
  ASSERT_EQ(1u, logs.size());
  EXPECT_EQ(JournalEntryType::kInstant, logs[0].type);
  EXPECT_EQ(task_id, logs[0].task_id);
  EXPECT_EQ("TestEvent", logs[0].event);
  ASSERT_EQ(1u, logs[0].details.size());
  EXPECT_EQ("key", logs[0].details[0].key);
  EXPECT_EQ("value", logs[0].details[0].value);
}

TEST_F(AggregatedJournalTest, GetLogsAsJson) {
  AggregatedJournal journal;
  ActorTaskId task_id(1);
  journal.Log(GURL("https://example.com"), task_id, "TestEvent",
              {{"key", "value"}});

  std::string json = journal.GetLogsAsJson();
  EXPECT_FALSE(json.empty());
  // Basic check for content.
  EXPECT_NE(std::string::npos, json.find("TestEvent"));
  EXPECT_NE(std::string::npos, json.find("key"));
  EXPECT_NE(std::string::npos, json.find("value"));
}

TEST_F(AggregatedJournalTest, AsyncEntry) {
  AggregatedJournal journal;
  ActorTaskId task_id(1);
  {
    auto pending =
        journal.CreatePendingAsyncEntry(GURL("https://example.com"), task_id,
                                        100, "AsyncEvent", {{"start", "now"}});
    auto logs = journal.GetLogs();
    ASSERT_EQ(1u, logs.size());
    EXPECT_EQ(JournalEntryType::kBegin, logs[0].type);
    EXPECT_EQ("AsyncEvent", logs[0].event);

    pending->EndEntry({{"end", "later"}});
  }

  auto logs = journal.GetLogs();
  ASSERT_EQ(2u, logs.size());
  EXPECT_EQ(JournalEntryType::kBegin, logs[0].type);
  EXPECT_EQ(JournalEntryType::kEnd, logs[1].type);
  EXPECT_EQ("AsyncEvent", logs[1].event);
  ASSERT_EQ(1u, logs[1].details.size());
  EXPECT_EQ("end", logs[1].details[0].key);
  EXPECT_EQ("later", logs[1].details[0].value);
}

TEST_F(AggregatedJournalTest, RingBuffer) {
  AggregatedJournal journal;
  ActorTaskId task_id(1);
  for (int i = 0; i < 30; ++i) {
    journal.Log(GURL(), task_id, "Event " + base::NumberToString(i), {});
  }

  auto logs = journal.GetLogs();
  // Buffer size is 20.
  EXPECT_EQ(20u, logs.size());
  EXPECT_EQ("Event 10", logs[0].event);
  EXPECT_EQ("Event 29", logs[19].event);
}

}  // namespace actor

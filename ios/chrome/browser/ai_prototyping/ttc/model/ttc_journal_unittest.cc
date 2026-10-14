// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

using ::testing::ElementsAre;

class TTCJournalTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Test logging an instant event and formatting its line.
TEST_F(TTCJournalTest, TestLogInstantEventAndFormatLine) {
  TTCJournal journal;
  journal.Log(TTCJournalComponent::kAudio, "CaptureStarted",
              TTCJournalDetailsBuilder()
                  .Add("sample_rate", 16000)
                  .Add("channels", 1)
                  .Build());

  ASSERT_EQ(journal.entries().size(), 1u);
  const TTCJournalEntry& entry = journal.entries()[0];
  EXPECT_EQ(entry.id, 1u);
  EXPECT_EQ(entry.component, TTCJournalComponent::kAudio);
  EXPECT_EQ(entry.type, TTCJournalEntryType::kInstant);
  EXPECT_EQ(entry.event_name, "CaptureStarted");
  EXPECT_THAT(entry.details,
              ElementsAre(TTCJournalDetail{"sample_rate", "16000"},
                          TTCJournalDetail{"channels", "1"}));

  std::string formatted = entry.FormatLine();
  EXPECT_NE(formatted.find("[Audio] CaptureStarted (sample_rate=16000, "
                           "channels=1)"),
            std::string::npos);
}

// Test that BeginAsyncEvent logs a kBegin entry and EndEntry logs a kEnd entry
// with elapsed duration_ms.
TEST_F(TTCJournalTest, TestAsyncEventExplicitEndEntryRecordsDuration) {
  TTCJournal journal;
  std::unique_ptr<TTCJournal::PendingAsyncEntry> async_event =
      journal.BeginAsyncEvent(
          TTCJournalComponent::kActuation, "ActuationExecute",
          TTCJournalDetailsBuilder().Add("task_id", 7).Build());

  ASSERT_EQ(journal.entries().size(), 1u);
  EXPECT_EQ(journal.entries()[0].type, TTCJournalEntryType::kBegin);
  EXPECT_EQ(journal.entries()[0].component, TTCJournalComponent::kActuation);
  EXPECT_EQ(journal.entries()[0].event_name, "ActuationExecute");

  task_environment_.FastForwardBy(base::Milliseconds(145));
  async_event->EndEntry(TTCJournalDetailsBuilder().Add("result", "ok").Build());

  ASSERT_EQ(journal.entries().size(), 2u);
  const TTCJournalEntry& end_entry = journal.entries()[1];
  EXPECT_EQ(end_entry.type, TTCJournalEntryType::kEnd);
  EXPECT_EQ(end_entry.component, TTCJournalComponent::kActuation);
  EXPECT_EQ(end_entry.event_name, "ActuationExecute");
  EXPECT_THAT(end_entry.details,
              ElementsAre(TTCJournalDetail{"result", "ok"},
                          TTCJournalDetail{"duration_ms", "145"}));
}

// Test that Clear() invalidates in-flight PendingAsyncEntry handles so they do
// not append an orphan kEnd entry after the journal is cleared.
TEST_F(TTCJournalTest, TestClearInvalidatesInFlightAsyncEvents) {
  TTCJournal journal;
  base::WeakPtr<TTCJournal> journal_weak = journal.GetWeakPtr();
  std::unique_ptr<TTCJournal::PendingAsyncEntry> async_event =
      journal.BeginAsyncEvent(TTCJournalComponent::kActuation,
                              "ActuationExecute");
  ASSERT_EQ(journal.entries().size(), 1u);

  journal.Clear();
  EXPECT_TRUE(journal.entries().empty());
  EXPECT_TRUE(journal_weak);

  async_event->EndEntry();
  async_event.reset();
  EXPECT_TRUE(journal.entries().empty());
}

// Test that entry-added and cleared listeners are notified as events are logged
// and cleared.
TEST_F(TTCJournalTest, TestListenersNotifiedOnAddAndClear) {
  TTCJournal journal;
  std::vector<std::string> added_events;
  int cleared_count = 0;

  base::CallbackListSubscription add_sub =
      journal.AddEntryAddedListener(base::BindRepeating(
          [](std::vector<std::string>* events, const TTCJournalEntry& entry) {
            events->push_back(entry.event_name);
          },
          base::Unretained(&added_events)));
  base::CallbackListSubscription clear_sub =
      journal.AddClearedListener(base::BindRepeating(
          [](int* count) { ++(*count); }, base::Unretained(&cleared_count)));

  journal.Log(TTCJournalComponent::kSession, "SessionStart");
  journal.Log(TTCJournalComponent::kAudio, "CaptureStarted");
  EXPECT_THAT(added_events, ElementsAre("SessionStart", "CaptureStarted"));
  EXPECT_EQ(cleared_count, 0);

  journal.Clear();
  EXPECT_TRUE(journal.entries().empty());
  EXPECT_EQ(cleared_count, 1);
}

// Test simplified formatting with SetSummary and in-place entry updates.
TEST_F(TTCJournalTest, TestSimplifiedSummaryAndUpdateEntry) {
  TTCJournal journal;
  uint64_t entry_id =
      journal.Log(TTCJournalComponent::kConversation, "TtcInputTranscription",
                  TTCJournalDetailsBuilder()
                      .SetSummary("Input: Hello")
                      .Add("text", "Hello")
                      .Build());
  journal.Log(TTCJournalComponent::kBackend, "ClockSync",
              TTCJournalDetailsBuilder().Add("clock_delta", "10ms").Build());
  journal.LogError(
      TTCJournalComponent::kBackend, "TtcBackendError",
      TTCJournalDetailsBuilder().SetSummary("Backend error").Build());
  journal.LogError(TTCJournalComponent::kBackend, "InternalServerError");

  ASSERT_EQ(journal.entries().size(), 4u);
  EXPECT_EQ(journal.entries()[0].FormatSimplifiedLine(), "Input: Hello");
  EXPECT_EQ(journal.entries()[1].FormatSimplifiedLine(), "");
  EXPECT_EQ(journal.entries()[2].FormatSimplifiedLine(),
            "[ERROR] Backend error");
  EXPECT_EQ(journal.entries()[3].FormatSimplifiedLine(), "");

  EXPECT_TRUE(
      journal.UpdateEntry(entry_id, TTCJournalDetailsBuilder()
                                        .SetSummary("Input: Hello world")
                                        .Add("text", "Hello world")
                                        .Build()));
  EXPECT_EQ(journal.entries()[0].FormatSimplifiedLine(), "Input: Hello world");
  EXPECT_FALSE(journal.UpdateEntry(999, {}));
}

}  // namespace

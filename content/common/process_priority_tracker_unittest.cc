// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/process_priority_tracker.h"

#include "base/process/process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using ::testing::Mock;

class MockProcessPriorityObserver
    : public ProcessPriorityTracker::ProcessPriorityObserver {
 public:
  MOCK_METHOD(void,
              OnPriorityChanged,
              (base::Process::Priority process_priority),
              (override));
};

// The tracker is a process-wide singleton, so its state carries over between
// tests. Each test sets a known priority before adding observers.
class ProcessPriorityTrackerTest : public testing::Test {
 protected:
  ProcessPriorityTracker& tracker() {
    return *ProcessPriorityTracker::GetInstance();
  }
};

TEST_F(ProcessPriorityTrackerTest, NotifiesObservers) {
  tracker().OnProcessPriorityChanged(base::Process::Priority::kUserBlocking);

  testing::StrictMock<MockProcessPriorityObserver> observer;
  // The current priority is replayed when the observer is added.
  EXPECT_CALL(observer,
              OnPriorityChanged(base::Process::Priority::kUserBlocking));
  tracker().AddObserver(&observer);
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_CALL(observer,
              OnPriorityChanged(base::Process::Priority::kBestEffort));
  tracker().OnProcessPriorityChanged(base::Process::Priority::kBestEffort);
  Mock::VerifyAndClearExpectations(&observer);

  // Repeated calls with the same priority are ignored.
  tracker().OnProcessPriorityChanged(base::Process::Priority::kBestEffort);

  tracker().RemoveObserver(&observer);

  // A removed observer isn't notified.
  tracker().OnProcessPriorityChanged(base::Process::Priority::kUserBlocking);
}

}  // namespace

}  // namespace content

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_
#define NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class FileNetLogObserver;

// Inherit from this class if a TaskEnvironment is needed in a test.
// Use in class hierachies where inheritance from ::testing::Test at the same
// time is not desirable or possible (for example, when inheriting from
// PlatformTest at the same time).
class WithTaskEnvironment {
 public:
  WithTaskEnvironment(const WithTaskEnvironment&) = delete;
  WithTaskEnvironment& operator=(const WithTaskEnvironment&) = delete;

 protected:
  // Always uses MainThreadType::IO, |time_source| may optionally be provided
  // to mock time.
  explicit WithTaskEnvironment(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT);

  ~WithTaskEnvironment();

  [[nodiscard]] bool MainThreadIsIdle() const {
    return task_environment_.MainThreadIsIdle();
  }

  [[nodiscard]] base::RepeatingClosure QuitClosure() {
    return task_environment_.QuitClosure();
  }

  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  // Only valid for instances using TimeSource::MOCK_TIME.
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  [[nodiscard]] const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

  [[nodiscard]] size_t GetPendingMainThreadTaskCount() const {
    return task_environment_.GetPendingMainThreadTaskCount();
  }

  [[nodiscard]] base::TimeDelta NextMainThreadPendingTaskDelay() const {
    return task_environment_.NextMainThreadPendingTaskDelay();
  }

 private:
  void MaybeStartNetLog();

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FileNetLogObserver> file_net_log_observer_;
};

// Inherit from this class instead of ::testing::Test directly if a
// TaskEnvironment is needed in a test.
class TestWithTaskEnvironment : public ::testing::Test,
                                public WithTaskEnvironment {
 public:
  TestWithTaskEnvironment(const TestWithTaskEnvironment&) = delete;
  TestWithTaskEnvironment& operator=(const TestWithTaskEnvironment&) = delete;

 protected:
  using WithTaskEnvironment::WithTaskEnvironment;
};

}  // namespace net

#endif  // NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_

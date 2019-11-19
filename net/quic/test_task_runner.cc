// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_task_runner.h"

#include <algorithm>
#include <utility>

#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {

namespace {

base::TimeTicks NowInTicks(const quic::MockClock& clock) {
  base::TimeTicks ticks;
  return ticks + base::TimeDelta::FromMicroseconds(
                     (clock.Now() - quic::QuicTime::Zero()).ToMicroseconds());
}

}  // namespace

TestTaskRunner::TestTaskRunner(quic::MockClock* clock) : clock_(clock) {}

TestTaskRunner::~TestTaskRunner() {}

bool TestTaskRunner::PostDelayedTask(const base::Location& from_here,
                                     base::OnceClosure task,
                                     base::TimeDelta delay) {
  EXPECT_GE(delay, base::TimeDelta());
  tasks_.push_back(PostedTask(from_here, std::move(task), NowInTicks(*clock_),
                              delay, base::TestPendingTask::NESTABLE));
  return false;
}

bool TestTaskRunner::PostNonNestableDelayedTask(const base::Location& from_here,
                                                base::OnceClosure task,
                                                base::TimeDelta delay) {
  return PostDelayedTask(from_here, std::move(task), delay);
}

bool TestTaskRunner::RunsTasksInCurrentSequence() const {
  return true;
}

const std::vector<PostedTask>& TestTaskRunner::GetPostedTasks() const {
  return tasks_;
}

quic::QuicTime::Delta TestTaskRunner::NextPendingTaskDelay() {
  if (tasks_.empty())
    return quic::QuicTime::Delta::Infinite();

  auto next = FindNextTask();
  return quic::QuicTime::Delta::FromMicroseconds(
      (next->GetTimeToRun() - NowInTicks(*clock_)).InMicroseconds());
}

void TestTaskRunner::RunNextTask() {
  auto next = FindNextTask();
  DCHECK(next != tasks_.end());
  clock_->AdvanceTime(quic::QuicTime::Delta::FromMicroseconds(
      (next->GetTimeToRun() - NowInTicks(*clock_)).InMicroseconds()));
  PostedTask task = std::move(*next);
  tasks_.erase(next);
  std::move(task.task).Run();
}

void TestTaskRunner::FastForwardBy(quic::QuicTime::Delta delta) {
  DCHECK_GE(delta, quic::QuicTime::Delta::Zero());

  quic::QuicTime end_timestamp = clock_->Now() + delta;

  while (NextPendingTaskDelay() <= end_timestamp - clock_->Now()) {
    RunNextTask();
  }

  if (clock_->Now() != end_timestamp)
    clock_->AdvanceTime(end_timestamp - clock_->Now());

  while (NextPendingTaskDelay() <= quic::QuicTime::Delta::Zero()) {
    RunNextTask();
  }
  return;
}

void TestTaskRunner::RunUntilIdle() {
  while (!tasks_.empty())
    RunNextTask();
}
namespace {

struct ShouldRunBeforeLessThan {
  bool operator()(const PostedTask& task1, const PostedTask& task2) const {
    return task1.ShouldRunBefore(task2);
  }
};

}  // namespace

std::vector<PostedTask>::iterator TestTaskRunner::FindNextTask() {
  return std::min_element(tasks_.begin(), tasks_.end(),
                          ShouldRunBeforeLessThan());
}

}  // namespace test
}  // namespace net

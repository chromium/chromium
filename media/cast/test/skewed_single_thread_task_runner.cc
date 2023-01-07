// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/skewed_single_thread_task_runner.h"

#include <utility>

#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace cast {
namespace test {

SkewedSingleThreadTaskRunner::SkewedSingleThreadTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) :
    skew_(1.0),
    task_runner_(task_runner) {
}

SkewedSingleThreadTaskRunner::~SkewedSingleThreadTaskRunner() = default;

void SkewedSingleThreadTaskRunner::SetSkew(double skew) {
  skew_ = skew;
}

bool SkewedSingleThreadTaskRunner::PostDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostDelayedTask(
      from_here, std::move(task),
      base::Microseconds(delay.InMicroseconds() * skew_));
}

bool SkewedSingleThreadTaskRunner::RunsTasksInCurrentSequence() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

bool SkewedSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  return task_runner_->PostNonNestableDelayedTask(
      from_here, std::move(task),
      base::Microseconds(delay.InMicroseconds() * skew_));
}

}  // namespace test
}  // namespace cast
}  // namespace media

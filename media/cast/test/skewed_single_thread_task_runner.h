// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_SKEWED_SINGLE_THREAD_TASK_RUNNER_H_
#define MEDIA_CAST_TEST_SKEWED_SINGLE_THREAD_TASK_RUNNER_H_

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_pending_task.h"

namespace media {
namespace cast {
namespace test {

// This class wraps a SingleThreadTaskRunner, and allows you to scale
// the delay for any posted task by a factor. The factor is changed by
// calling SetSkew(). A skew of 2.0 means that all delayed task will
// have to wait twice as long.
class SkewedSingleThreadTaskRunner final : public base::SingleThreadTaskRunner {
 public:
  explicit SkewedSingleThreadTaskRunner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  SkewedSingleThreadTaskRunner(const SkewedSingleThreadTaskRunner&) = delete;
  SkewedSingleThreadTaskRunner& operator=(const SkewedSingleThreadTaskRunner&) =
      delete;

  // Set the delay multiplier to |skew|.
  void SetSkew(double skew);

  // base::SingleThreadTaskRunner implementation.
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) final;

  bool RunsTasksInCurrentSequence() const final;

  // This function is currently not used, and will return false.
  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) final;

 protected:
  ~SkewedSingleThreadTaskRunner() final;

 private:
  double skew_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace test
}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_SKEWED_SINGLE_THREAD_TASK_RUNNER_H_

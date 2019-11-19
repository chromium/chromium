// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_QUEUEING_TIME_ESTIMATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_QUEUEING_TIME_ESTIMATOR_H_

#include <array>
#include <vector>

#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/main_thread_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace scheduler {

// Records the expected queueing time for a high priority task occurring
// randomly during each interval of length equal to window's duration.
class PLATFORM_EXPORT QueueingTimeEstimator {
  DISALLOW_NEW();

 public:
  class PLATFORM_EXPORT Client {
   public:
    virtual void OnQueueingTimeForWindowEstimated(base::TimeDelta queueing_time,
                                                  bool is_disjoint_window) = 0;
    Client() = default;
    virtual ~Client() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Client);
  };

  class RunningAverage {
    DISALLOW_NEW();

   public:
    explicit RunningAverage(int steps_per_window);
    int GetStepsPerWindow() const;
    void Add(base::TimeDelta bin_value);
    base::TimeDelta GetAverage() const;
    bool IndexIsZero() const;

   private:
    size_t index_;
    std::vector<base::TimeDelta> circular_buffer_;
    base::TimeDelta running_sum_;
  };

  class PLATFORM_EXPORT Calculator {
    DISALLOW_NEW();

   public:
    explicit Calculator(int steps_per_window);

    void AddQueueingTime(base::TimeDelta queuing_time);
    void EndStep(Client* client);
    void ResetStep();

   private:
    // Variables to compute the total Expected Queueing Time.
    // |steps_per_window_| is the ratio of window duration to the sliding
    // window's step width. It is an integer since the window must be a integer
    // multiple of the step's width. This parameter is used for deciding the
    // sliding window's step width, and the number of bins of the circular
    // buffer.
    const int steps_per_window_;

    // |step_expected_queueing_time_| is the expected queuing time of a
    // smaller window of a step's width. By combining these step EQTs through a
    // running average, we can get window EQTs of a bigger window.
    //
    // ^ Instantaneous queuing time
    // |
    // |
    // |   |\                                           .
    // |   | \            |\             |\             .
    // |   |  \           | \       |\   | \            .
    // |   |   \    |\    |  \      | \  |  \           .
    // |   |    \   | \   |   \     |  \ |   \          .
    // ------------------------------------------------> Time
    //
    // |stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|stepEQT|
    //
    // |------windowEQT_1------|
    //         |------windowEQT_2------|
    //                 |------windowEQT_3------|
    //
    // In this case:
    // |steps_per_window_| = 3, because each window is the length of 3 steps.
    base::TimeDelta step_expected_queueing_time_;
    RunningAverage sliding_window_;
  };

  QueueingTimeEstimator(Client* client,
                        base::TimeDelta window_duration,
                        int steps_per_window,
                        bool start_disabled);

  void OnExecutionStarted(base::TimeTicks now);
  void OnExecutionStopped(base::TimeTicks now);
  void OnRecordingStateChanged(bool disabled, base::TimeTicks transition_time);

 private:
  void AdvanceTime(base::TimeTicks current_time);
  bool TimePastStepEnd(base::TimeTicks task_end_time);

  Client* client_;  // NOT OWNED.
  const base::TimeDelta window_step_width_;
  base::TimeTicks step_start_time_;

  // Tasks that arrive in the busy period need to wait in a task queue.
  bool busy_ = false;
  base::TimeTicks busy_period_start_time_;

  // |disabled_| is true iff we want to ignore start/stop events.
  bool disabled_;
  Calculator calculator_;

  DISALLOW_ASSIGN(QueueingTimeEstimator);
};

}  // namespace scheduler
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_SCHEDULER_MAIN_THREAD_QUEUEING_TIME_ESTIMATOR_H_

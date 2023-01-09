// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SPEED_LIMIT_UMA_LISTENER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SPEED_LIMIT_UMA_LISTENER_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Tracks the OS speed limit and logs to UMA histograms.
class MODULES_EXPORT SpeedLimitUmaListener {
 public:
  static constexpr base::TimeDelta kStatsReportingPeriod = base::Minutes(1);

  explicit SpeedLimitUmaListener(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SpeedLimitUmaListener();

  void OnSpeedLimitChange(int32_t speed_limit);

 private:
  void ReportStats();
  void ScheduleReport();

  base::Lock lock_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  int32_t current_speed_limit_ GUARDED_BY(&lock_);
  int32_t num_throttling_episodes_ GUARDED_BY(&lock_) = 0;
  base::WeakPtrFactory<SpeedLimitUmaListener> weak_ptr_factory_;
};

}  //  namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_SPEED_LIMIT_UMA_LISTENER_H_

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/test/recording_task_time_observer.h"

#include "base/logging.h"

namespace blink {
namespace scheduler {

RecordingTaskTimeObserver::RecordingTaskTimeObserver() = default;
RecordingTaskTimeObserver::~RecordingTaskTimeObserver() = default;

void RecordingTaskTimeObserver::Clear() {
  result_.clear();
}

void RecordingTaskTimeObserver::WillProcessTask(base::TimeTicks start_time) {
  result_.emplace_back(start_time, base::TimeTicks());
}

void RecordingTaskTimeObserver::DidProcessTask(base::TimeTicks start_time,
                                               base::TimeTicks end_time) {
  DCHECK(!result_.IsEmpty());
  DCHECK_EQ(result_.back().first, start_time);
  result_.back().second = end_time;
}

}  // namespace scheduler
}  // namespace blink

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/long_task_detector.h"

#include "base/time/time.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

constexpr base::TimeDelta LongTaskDetector::kLongTaskThreshold;

// static
LongTaskDetector& LongTaskDetector::Instance() {
  DEFINE_STATIC_LOCAL(Persistent<LongTaskDetector>, long_task_detector,
                      (MakeGarbageCollected<LongTaskDetector>()));
  DCHECK(IsMainThread());
  return *long_task_detector;
}

LongTaskDetector::LongTaskDetector() = default;

void LongTaskDetector::RegisterObserver(LongTaskObserver* observer) {
  DCHECK(IsMainThread());
  DCHECK(observer);
  DCHECK(!iterating_);
  if (observers_.insert(observer).is_new_entry && observers_.size() == 1) {
    // Number of observers just became non-zero.
    Thread::Current()->AddTaskTimeObserver(this);
  }
}

void LongTaskDetector::UnregisterObserver(LongTaskObserver* observer) {
  DCHECK(IsMainThread());
  if (iterating_) {
    observers_to_be_removed_.push_back(observer);
    return;
  }
  observers_.erase(observer);
  if (observers_.size() == 0) {
    Thread::Current()->RemoveTaskTimeObserver(this);
  }
}

void LongTaskDetector::DidProcessTask(base::TimeTicks start_time,
                                      base::TimeTicks end_time) {
  if ((end_time - start_time) < LongTaskDetector::kLongTaskThreshold)
    return;

  iterating_ = true;
  for (auto& observer : observers_) {
    observer->OnLongTaskDetected(start_time, end_time);
  }
  iterating_ = false;

  for (const auto& observer : observers_to_be_removed_) {
    UnregisterObserver(observer);
  }
  observers_to_be_removed_.clear();
}

void LongTaskDetector::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
  visitor->Trace(observers_to_be_removed_);
}

}  // namespace blink

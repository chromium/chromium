// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/long_task_detector.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

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
  if (observers_.insert(observer).is_new_entry && observers_.size() == 1) {
    // Number of observers just became non-zero.
    Thread::Current()->AddTaskTimeObserver(this);
  }
}

void LongTaskDetector::UnregisterObserver(LongTaskObserver* observer) {
  DCHECK(IsMainThread());
  observers_.erase(observer);
  if (observers_.size() == 0) {
    Thread::Current()->RemoveTaskTimeObserver(this);
  }
}

void LongTaskDetector::DidProcessTask(base::TimeTicks start_time,
                                      base::TimeTicks end_time) {
  if ((end_time - start_time) < LongTaskDetector::kLongTaskThreshold)
    return;

  // We copy `observers_` because it might be mutated in OnLongTaskDetected,
  // and container mutation is not allowed during iteration.
  const HeapHashSet<Member<LongTaskObserver>> observers = observers_;
  for (auto& observer : observers) {
    observer->OnLongTaskDetected(start_time, end_time);
  }
}

void LongTaskDetector::Trace(Visitor* visitor) const {
  visitor->Trace(observers_);
}

}  // namespace blink

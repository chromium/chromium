// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LONG_TASK_DETECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LONG_TASK_DETECTOR_H_

#include "base/macros.h"
#include "base/task/sequence_manager/task_time_observer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"

namespace blink {

class CORE_EXPORT LongTaskObserver : public GarbageCollectedMixin {
 public:
  virtual ~LongTaskObserver() = default;

  virtual void OnLongTaskDetected(base::TimeTicks start_time,
                                  base::TimeTicks end_time) = 0;
};

// LongTaskDetector detects tasks longer than kLongTaskThreshold and notifies
// observers. When it has non-zero LongTaskObserver, it adds itself as a
// TaskTimeObserver on the main thread and observes every task. When the number
// of LongTaskObservers drop to zero it automatically removes itself as a
// TaskTimeObserver.
class CORE_EXPORT LongTaskDetector final
    : public GarbageCollected<LongTaskDetector>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  static LongTaskDetector& Instance();

  LongTaskDetector();

  void RegisterObserver(LongTaskObserver*);
  void UnregisterObserver(LongTaskObserver*);

  void Trace(blink::Visitor*);

  static constexpr base::TimeDelta kLongTaskThreshold =
      base::TimeDelta::FromMilliseconds(50);

 private:
  // scheduler::TaskTimeObserver implementation
  void WillProcessTask(base::TimeTicks start_time) override {}
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override;

  HeapHashSet<Member<LongTaskObserver>> observers_;

  DISALLOW_COPY_AND_ASSIGN(LongTaskDetector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_LONG_TASK_DETECTOR_H_

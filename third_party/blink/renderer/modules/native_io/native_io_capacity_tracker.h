// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_CAPACITY_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_CAPACITY_TRACKER_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// Tracks the capacity allocated to an execution context.
class NativeIOCapacityTracker final
    : public GarbageCollected<NativeIOCapacityTracker> {
 public:
  NativeIOCapacityTracker() = default;

  NativeIOCapacityTracker(const NativeIOCapacityTracker&) = delete;
  NativeIOCapacityTracker& operator=(const NativeIOCapacityTracker&) = delete;

  // Changes the available capacity by the specified amount if possible.
  //
  // Returns true if and only if the capacity change is successfully applied.
  // If `capacity_delta` < 0, returns false if the change would bring the
  // available capacity below 0.
  bool ChangeAvailableCapacity(int64_t capacity_delta);

  // Returns the available capacity for this capacity tracker.
  uint64_t GetAvailableCapacity() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return available_capacity;
  }

  // GarbageCollected
  void Trace(Visitor*) const {}

 private:
  int64_t available_capacity GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_CAPACITY_TRACKER_H_

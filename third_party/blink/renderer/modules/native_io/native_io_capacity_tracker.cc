// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

bool NativeIOCapacityTracker::ChangeAvailableCapacity(int64_t capacity_delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int64_t new_capacity;
  if (!base::CheckAdd(available_capacity, capacity_delta)
           .AssignIfValid(&new_capacity)) {
    return false;
  }

  if (new_capacity < 0)
    return false;
  available_capacity = new_capacity;
  return true;
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/test_memory_tracker.h"

#include "base/check.h"

namespace gpu {

void TestMemoryTracker::TrackMemoryAllocatedChange(int64_t delta) {
  CHECK(delta >= 0 || current_size_ >= static_cast<uint64_t>(-delta));
  current_size_ += delta;
}

uint64_t TestMemoryTracker::GetSize() const {
  return current_size_;
}

uint64_t TestMemoryTracker::ClientTracingId() const {
  return 0;
}

int TestMemoryTracker::ClientId() const {
  return 0;
}

uint64_t TestMemoryTracker::ContextGroupTracingId() const {
  return 0;
}

}  // namespace gpu

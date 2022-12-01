// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEST_MEMORY_TRACKER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEST_MEMORY_TRACKER_H_

#include "gpu/command_buffer/service/memory_tracking.h"

namespace gpu {

class TestMemoryTracker : public MemoryTracker {
 public:
  TestMemoryTracker() = default;
  TestMemoryTracker(const TestMemoryTracker&) = delete;
  TestMemoryTracker& operator=(const TestMemoryTracker&) = delete;
  ~TestMemoryTracker() override = default;

  // MemoryTracker implementation.
  void TrackMemoryAllocatedChange(int64_t delta) override;
  uint64_t GetSize() const override;
  uint64_t ClientTracingId() const override;
  int ClientId() const override;
  uint64_t ContextGroupTracingId() const override;

 private:
  uint64_t current_size_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEST_MEMORY_TRACKER_H_

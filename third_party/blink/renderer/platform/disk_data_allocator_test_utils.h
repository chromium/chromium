// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_TEST_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_TEST_UTILS_H_

#include "third_party/blink/renderer/platform/disk_data_allocator.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>

#include "base/synchronization/lock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class InMemoryDataAllocator : public DiskDataAllocator {
 public:
  constexpr static size_t kMaxSize = 1 << 20;

  InMemoryDataAllocator() : data_(kMaxSize) { set_may_write_for_testing(true); }
  ~InMemoryDataAllocator() override = default;

  std::map<int64_t, size_t> FreeChunks() {
    base::AutoLock locker(lock_);

    size_t free_size = 0;
    for (const auto& p : free_chunks_)
      free_size += p.second;

    EXPECT_EQ(free_size, free_chunks_size_);

    return free_chunks_;
  }

 private:
  int DoWrite(int64_t offset, const char* data, int size) override {
    int64_t end_offset = offset + size;
    if (static_cast<size_t>(end_offset) > kMaxSize)
      return -1;

    memcpy(&data_[0] + offset, data, size);
    max_offset_ = std::max(end_offset, max_offset_);
    return size;
  }

  void DoRead(int64_t offset, char* data, int size) override {
    int64_t end_offset = offset + size;
    ASSERT_LE(end_offset, max_offset_);

    memcpy(data, &data_[0] + offset, size);
  }

 private:
  int64_t max_offset_ = 0;
  Vector<char> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_DISK_DATA_ALLOCATOR_TEST_UTILS_H_

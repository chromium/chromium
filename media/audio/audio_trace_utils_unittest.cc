// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_trace_utils.h"

#include <cstddef>
#include <cstdint>

#include "base/memory/unsafe_shared_memory_region.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr size_t kRegionSize = 1024;
constexpr uint32_t kBufferIndex = 42;

}  // namespace

// Makes sure both ends of a stream get the same flow IDs from their handles to
// the shared memory region, and that other regions and buffers get other IDs.
TEST(AudioTraceUtilsTest, IdsMatchForSameRegion) {
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  // The renderer gets its own handle to the audio service's region.
  base::UnsafeSharedMemoryRegion renderer_region = region.Duplicate();
  ASSERT_TRUE(renderer_region.IsValid());
  base::UnsafeSharedMemoryRegion other_region =
      base::UnsafeSharedMemoryRegion::Create(kRegionSize);
  ASSERT_TRUE(other_region.IsValid());

  const uint64_t base_id = GetAudioTraceFlowBaseId(region.GetGUID());
  EXPECT_EQ(GetAudioTraceFlowBaseId(renderer_region.GetGUID()), base_id);
  EXPECT_NE(GetAudioTraceFlowBaseId(other_region.GetGUID()), base_id);
  EXPECT_NE(GetAudioTraceFlowId(base_id, kBufferIndex),
            GetAudioTraceFlowId(base_id, kBufferIndex + 1));
}

}  // namespace media

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/frame_buffer_pool.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

constexpr size_t kBufferSize = 1024;

TEST(FrameBufferPool, BasicFunctionality) {
  auto pool = base::MakeRefCounted<FrameBufferPool>();

  void* priv1 = nullptr;
  uint8_t* buf1 = pool->GetFrameBuffer(kBufferSize, &priv1);
  ASSERT_TRUE(priv1);
  ASSERT_TRUE(buf1);
  memset(buf1, 0, kBufferSize);

  void* priv2 = nullptr;
  uint8_t* buf2 = pool->GetFrameBuffer(kBufferSize, &priv2);
  ASSERT_TRUE(priv2);
  ASSERT_TRUE(buf2);
  EXPECT_NE(priv1, priv2);
  EXPECT_NE(buf1, buf2);
  memset(buf2, 0, kBufferSize);

  uint8_t* alpha = pool->AllocateAlphaPlaneForFrameBuffer(kBufferSize, priv1);
  ASSERT_TRUE(alpha);
  EXPECT_NE(alpha, buf1);
  EXPECT_NE(alpha, buf2);
  memset(alpha, 0, kBufferSize);

  EXPECT_EQ(2u, pool->get_pool_size_for_testing());

  // Frames are not released immediately, so this should still show two frames.
  pool->ReleaseFrameBuffer(priv2);
  priv2 = buf2 = nullptr;
  EXPECT_EQ(2u, pool->get_pool_size_for_testing());

  auto frame_release_cb = pool->CreateFrameCallback(priv1);

  // Shutdown should release all memory that's not held by VideoFrames.
  pool->Shutdown();
  EXPECT_EQ(1u, pool->get_pool_size_for_testing());

  memset(buf1, 0, kBufferSize);
  memset(alpha, 0, kBufferSize);

  // This will release all memory since we're in the shutdown state.
  std::move(frame_release_cb).Run();
  EXPECT_EQ(0u, pool->get_pool_size_for_testing());
}

TEST(FrameBufferPool, ForceAllocationError) {
  auto pool = base::MakeRefCounted<FrameBufferPool>();

  pool->force_allocation_error_for_testing();

  void* priv1 = nullptr;
  uint8_t* buf1 = pool->GetFrameBuffer(kBufferSize, &priv1);
  ASSERT_FALSE(priv1);
  ASSERT_FALSE(buf1);
  pool->Shutdown();
}

TEST(FrameBufferPool, DeferredDestruction) {
  auto pool = base::MakeRefCounted<FrameBufferPool>();

  base::SimpleTestTickClock test_clock;
  pool->set_tick_clock_for_testing(&test_clock);

  void* priv1 = nullptr;
  uint8_t* buf1 = pool->GetFrameBuffer(kBufferSize, &priv1);
  void* priv2 = nullptr;
  uint8_t* buf2 = pool->GetFrameBuffer(kBufferSize, &priv2);
  void* priv3 = nullptr;
  uint8_t* buf3 = pool->GetFrameBuffer(kBufferSize, &priv3);
  EXPECT_EQ(3u, pool->get_pool_size_for_testing());

  auto frame_release_cb = pool->CreateFrameCallback(priv1);
  pool->ReleaseFrameBuffer(priv1);
  priv1 = buf1 = nullptr;
  std::move(frame_release_cb).Run();

  // Frame buffers should not be immediately deleted upon return.
  EXPECT_EQ(3u, pool->get_pool_size_for_testing());

  // Advance some time, but not enough to trigger expiration.
  test_clock.Advance(base::Seconds(FrameBufferPool::kStaleFrameLimitSecs / 2));

  // We should still have 3 frame buffers in the pool at this point.
  frame_release_cb = pool->CreateFrameCallback(priv2);
  pool->ReleaseFrameBuffer(priv2);
  priv2 = buf2 = nullptr;
  std::move(frame_release_cb).Run();
  EXPECT_EQ(3u, pool->get_pool_size_for_testing());

  test_clock.Advance(base::Seconds(FrameBufferPool::kStaleFrameLimitSecs + 1));

  // All but this most recently released frame should remain now.
  frame_release_cb = pool->CreateFrameCallback(priv3);
  pool->ReleaseFrameBuffer(priv3);
  priv3 = buf3 = nullptr;
  std::move(frame_release_cb).Run();
  EXPECT_EQ(1u, pool->get_pool_size_for_testing());

  pool->Shutdown();
}

TEST(FrameBufferPool, DoesClearAllocations) {
  auto pool = base::MakeRefCounted<FrameBufferPool>(
      /*zero_initialize_memory=*/true);

  // Certainly this is not foolproof, but even flaky failures here indicate that
  // something is broken.
  void* priv1 = nullptr;
  uint8_t* buf = pool->GetFrameBuffer(kBufferSize, &priv1);
  bool nonzero = false;
  for (size_t i = 0; i < kBufferSize; i++) {
    nonzero |= !!buf[i];
  }
  EXPECT_FALSE(nonzero);
  pool->Shutdown();
}

}  // namespace media

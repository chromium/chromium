// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/shared_impl/media_stream_buffer_manager.h"

#include <stdint.h>

#include <utility>

#include "base/memory/unsafe_shared_memory_region.h"
#include "ppapi/c/pp_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

base::UnsafeSharedMemoryRegion CreateSharedMemory(int32_t buffer_size,
                                                  int32_t number_of_buffers) {
  base::UnsafeSharedMemoryRegion region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size * number_of_buffers);
  EXPECT_TRUE(region.IsValid());
  return region;
}

}  // namespace

namespace ppapi {

class MockDelegate : public MediaStreamBufferManager::Delegate {
 public:
  MockDelegate() : new_buffer_enqueue_counter_(0) {}
  void OnNewBufferEnqueued() override {
    new_buffer_enqueue_counter_++;
  }

  int32_t new_buffer_enqueue_counter_;
};

TEST(MediaStreamBufferManager, General) {
  {
    const int32_t kNumberOfBuffers = 5;
    const int32_t kBufferSize = 128;
    MockDelegate delegate;
    MediaStreamBufferManager manager(&delegate);
    base::UnsafeSharedMemoryRegion shared_memory =
        CreateSharedMemory(kBufferSize, kNumberOfBuffers);
    // SetBuffers with enqueue_all_buffers = true;
    EXPECT_TRUE(manager.SetBuffers(kNumberOfBuffers, kBufferSize,
                                   std::move(shared_memory), true));

    int8_t* memory = reinterpret_cast<int8_t*>(manager.GetBufferPointer(0));
    EXPECT_NE(static_cast<int8_t*>(NULL), memory);

    EXPECT_EQ(kNumberOfBuffers, manager.number_of_buffers());
    EXPECT_EQ(kBufferSize, manager.buffer_size());
    EXPECT_TRUE(manager.HasAvailableBuffer());

    // Test DequeueBuffer() and GetBufferPointer()
    for (int32_t i = 0; i < kNumberOfBuffers; ++i) {
      EXPECT_EQ(i, manager.DequeueBuffer());
      EXPECT_EQ(reinterpret_cast<MediaStreamBuffer*>(memory + i * kBufferSize),
                manager.GetBufferPointer(i));
    }

    manager.EnqueueBuffer(0);
    manager.EnqueueBuffer(4);
    manager.EnqueueBuffer(3);
    manager.EnqueueBuffer(1);
    manager.EnqueueBuffer(2);
    EXPECT_EQ(5, delegate.new_buffer_enqueue_counter_);

    EXPECT_EQ(0, manager.DequeueBuffer());
    EXPECT_EQ(4, manager.DequeueBuffer());
    EXPECT_EQ(3, manager.DequeueBuffer());
    EXPECT_EQ(1, manager.DequeueBuffer());
    EXPECT_EQ(2, manager.DequeueBuffer());
    EXPECT_EQ(PP_ERROR_FAILED, manager.DequeueBuffer());
    EXPECT_EQ(PP_ERROR_FAILED, manager.DequeueBuffer());
    EXPECT_FALSE(manager.HasAvailableBuffer());

    // Returns NULL for invalid index to GetBufferPointer()
    EXPECT_EQ(NULL, manager.GetBufferPointer(-1));
    EXPECT_EQ(NULL, manager.GetBufferPointer(kNumberOfBuffers));

    // Test crash for passing invalid index to EnqueueBuffer().
    // String arguments aren't passed to CHECK() in official builds.
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
    EXPECT_DEATH(manager.EnqueueBuffer(-1), "");
    EXPECT_DEATH(manager.EnqueueBuffer(kNumberOfBuffers), "");
#else
    EXPECT_DEATH(manager.EnqueueBuffer(-1), ".*Check failed: index >= 0.*");
    EXPECT_DEATH(manager.EnqueueBuffer(kNumberOfBuffers),
                 ".*Check failed: index < number_of_buffers_.*");
#endif
  }

  {
    const int32_t kNumberOfBuffers = 5;
    const int32_t kBufferSize = 128;
    MockDelegate delegate;
    MediaStreamBufferManager manager(&delegate);
    base::UnsafeSharedMemoryRegion shared_memory =
        CreateSharedMemory(kBufferSize, kNumberOfBuffers);
    // SetBuffers with enqueue_all_buffers = false;
    EXPECT_TRUE(manager.SetBuffers(kNumberOfBuffers, kBufferSize,
                                   std::move(shared_memory), false));

    int8_t* memory = reinterpret_cast<int8_t*>(manager.GetBufferPointer(0));
    EXPECT_NE(static_cast<int8_t*>(NULL), memory);

    EXPECT_EQ(kNumberOfBuffers, manager.number_of_buffers());
    EXPECT_EQ(kBufferSize, manager.buffer_size());

    // Test DequeueBuffer() and GetBufferPointer()
    for (int32_t i = 0; i < kNumberOfBuffers; ++i) {
      EXPECT_EQ(PP_ERROR_FAILED, manager.DequeueBuffer());
      EXPECT_EQ(reinterpret_cast<MediaStreamBuffer*>(memory + i * kBufferSize),
                manager.GetBufferPointer(i));
    }
  }
}

TEST(MediaStreamBufferManager, ResetBuffers) {
  const int32_t kNumberOfBuffers1 = 5;
  const int32_t kBufferSize1 = 128;
  const int32_t kNumberOfBuffers2 = 8;
  const int32_t kBufferSize2 = 256;
  MockDelegate delegate;
  MediaStreamBufferManager manager(&delegate);
  {
    base::UnsafeSharedMemoryRegion shared_memory =
        CreateSharedMemory(kBufferSize1, kNumberOfBuffers1);
    // SetBuffers with enqueue_all_buffers = true;
    EXPECT_TRUE(manager.SetBuffers(kNumberOfBuffers1, kBufferSize1,
                                   std::move(shared_memory), true));

    int8_t* memory = reinterpret_cast<int8_t*>(manager.GetBufferPointer(0));
    EXPECT_NE(static_cast<int8_t*>(NULL), memory);

    EXPECT_EQ(kNumberOfBuffers1, manager.number_of_buffers());
    EXPECT_EQ(kBufferSize1, manager.buffer_size());

    // Test DequeueBuffer() and GetBufferPointer()
    for (int32_t i = 0; i < kNumberOfBuffers1; ++i) {
      EXPECT_EQ(i, manager.DequeueBuffer());
      EXPECT_EQ(reinterpret_cast<MediaStreamBuffer*>(memory + i * kBufferSize1),
                manager.GetBufferPointer(i));
    }
  }

  {
    base::UnsafeSharedMemoryRegion shared_memory =
        CreateSharedMemory(kBufferSize2, kNumberOfBuffers2);
    // SetBuffers with enqueue_all_buffers = true;
    EXPECT_TRUE(manager.SetBuffers(kNumberOfBuffers2, kBufferSize2,
                                   std::move(shared_memory), true));

    int8_t* memory = reinterpret_cast<int8_t*>(manager.GetBufferPointer(0));
    EXPECT_NE(static_cast<int8_t*>(NULL), memory);

    EXPECT_EQ(kNumberOfBuffers2, manager.number_of_buffers());
    EXPECT_EQ(kBufferSize2, manager.buffer_size());

    // Test DequeueBuffer() and GetBufferPointer()
    for (int32_t i = 0; i < kNumberOfBuffers2; ++i) {
      EXPECT_EQ(i, manager.DequeueBuffer());
      EXPECT_EQ(reinterpret_cast<MediaStreamBuffer*>(memory + i * kBufferSize2),
                manager.GetBufferPointer(i));
    }
  }
}

}  // namespace ppapi

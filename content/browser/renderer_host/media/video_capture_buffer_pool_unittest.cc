// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureBufferPool.

#include "media/capture/video/video_capture_buffer_pool.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/scoped_task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

static const media::VideoPixelFormat kCapturePixelFormats[] = {
    media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_ARGB, media::PIXEL_FORMAT_Y16,
};

static const int kTestBufferPoolSize = 3;

// Note that this test does not exercise the class VideoCaptureBufferPool
// in isolation. The "unit under test" is an instance of VideoCaptureBufferPool
// with some context that is specific to renderer_host/media, and therefore
// this test must live here and not in media/capture/video.
class VideoCaptureBufferPoolTest
    : public testing::TestWithParam<media::VideoPixelFormat> {
 protected:
  // This is a generic Buffer tracker
  class Buffer {
   public:
    Buffer(const scoped_refptr<media::VideoCaptureBufferPool> pool,
           std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle,
           int id)
        : id_(id), pool_(pool), buffer_handle_(std::move(buffer_handle)) {}
    ~Buffer() { pool_->RelinquishProducerReservation(id()); }
    int id() const { return id_; }
    size_t mapped_size() { return buffer_handle_->mapped_size(); }
    void* data() { return buffer_handle_->data(); }

   private:
    const int id_;
    const scoped_refptr<media::VideoCaptureBufferPool> pool_;
    const std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle_;
  };

  VideoCaptureBufferPoolTest()
      : expected_dropped_id_(0),
        pool_(new media::VideoCaptureBufferPoolImpl(
            std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(),
            kTestBufferPoolSize)) {}

  void ExpectDroppedId(int expected_dropped_id) {
    expected_dropped_id_ = expected_dropped_id;
  }

  std::unique_ptr<Buffer> ReserveBuffer(const gfx::Size& dimensions,
                                        media::VideoPixelFormat pixel_format) {
    // To verify that ReserveBuffer always sets |buffer_id_to_drop|,
    // initialize it to something different than the expected value.
    int buffer_id_to_drop = ~expected_dropped_id_;
    DVLOG(1) << media::VideoPixelFormatToString(pixel_format) << " "
             << dimensions.ToString();
    const int arbitrary_frame_feedback_id = 0;
    int buffer_id = media::VideoCaptureBufferPool::kInvalidId;
    const auto reserve_result = pool_->ReserveForProducer(
        dimensions, pixel_format, nullptr, arbitrary_frame_feedback_id,
        &buffer_id, &buffer_id_to_drop);
    if (reserve_result !=
        media::VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
      return std::unique_ptr<Buffer>();
    }
    EXPECT_EQ(expected_dropped_id_, buffer_id_to_drop);

    std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle =
        pool_->GetHandleForInProcessAccess(buffer_id);
    return std::unique_ptr<Buffer>(
        new Buffer(pool_, std::move(buffer_handle), buffer_id));
  }

  base::test::ScopedTaskEnvironment task_environment_;
  int expected_dropped_id_;
  scoped_refptr<media::VideoCaptureBufferPool> pool_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureBufferPoolTest);
};

TEST_P(VideoCaptureBufferPoolTest, BufferPool) {
  const gfx::Size size_lo = gfx::Size(10, 10);
  const gfx::Size size_hi = gfx::Size(21, 33);
  const media::VideoCaptureFormat format_lo(size_lo, 0.0, GetParam());
  const media::VideoCaptureFormat format_hi(size_hi, 0.0, GetParam());

  // Reallocation won't happen for the first part of the test.
  ExpectDroppedId(media::VideoCaptureBufferPool::kInvalidId);

  // The buffer pool should have zero utilization before any buffers have been
  // reserved.
  ASSERT_EQ(0.0, pool_->GetBufferPoolUtilization());

  std::unique_ptr<Buffer> buffer1 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer1.get());
  ASSERT_EQ(1.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer2 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer3 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer3.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());

  ASSERT_LE(format_lo.ImageAllocationSize(), buffer1->mapped_size());
  ASSERT_LE(format_lo.ImageAllocationSize(), buffer2->mapped_size());
  ASSERT_LE(format_lo.ImageAllocationSize(), buffer3->mapped_size());

  ASSERT_NE(nullptr, buffer1->data());
  ASSERT_NE(nullptr, buffer2->data());
  ASSERT_NE(nullptr, buffer3->data());

  // Touch the memory.
  if (buffer1->data() != nullptr)
    memset(buffer1->data(), 0x11, buffer1->mapped_size());
  if (buffer2->data() != nullptr)
    memset(buffer2->data(), 0x44, buffer2->mapped_size());
  if (buffer3->data() != nullptr)
    memset(buffer3->data(), 0x77, buffer3->mapped_size());

  // Fourth buffer should fail.  Buffer pool utilization should be at 100%.
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Release 1st buffer and retry; this should succeed.
  buffer1.reset();
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer4 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer4.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());

  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_hi, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Validate the IDs
  int buffer_id2 = buffer2->id();
  ASSERT_EQ(1, buffer_id2);
  const int buffer_id3 = buffer3->id();
  ASSERT_EQ(2, buffer_id3);
  const int buffer_id4 = buffer4->id();
  ASSERT_EQ(0, buffer_id4);
  void* const memory_pointer3 = buffer3->data();

  // Deliver a buffer.
  pool_->HoldForConsumers(buffer_id3, 2);

  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  buffer3.reset();  // Old producer releases buffer. Should be a noop.
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_hi, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  buffer2.reset();  // Active producer releases buffer. Should free a buffer.

  buffer1 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer1.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // First consumer finishes.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Second consumer finishes. This should free that buffer.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  buffer3 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer3.get());
  ASSERT_EQ(buffer_id3, buffer3->id()) << "Buffer ID should be reused.";
  ASSERT_EQ(memory_pointer3, buffer3->data());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Now deliver & consume buffer1, but don't release the buffer.
  int buffer_id1 = buffer1->id();
  ASSERT_EQ(1, buffer_id1);
  pool_->HoldForConsumers(buffer_id1, 5);
  pool_->RelinquishConsumerHold(buffer_id1, 5);

  // Even though the consumer is done with the buffer at |buffer_id1|, it cannot
  // be re-allocated to the producer, because |buffer1| still references it. But
  // when |buffer1| goes away, we should be able to re-reserve the buffer (and
  // the ID ought to be the same).
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  buffer1.reset();  // Should free the buffer.
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_lo, GetParam());
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(buffer_id1, buffer2->id());
  buffer_id2 = buffer_id1;
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Now try reallocation with different resolutions. We expect reallocation
  // to occur only when the old buffer is too small.
  buffer2.reset();
  ExpectDroppedId(buffer_id2);
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_hi, GetParam());
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_LE(format_hi.ImageAllocationSize(), buffer2->mapped_size());
  ASSERT_EQ(3, buffer2->id());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  void* const memory_pointer_hi = buffer2->data();
  buffer2.reset();  // Frees it.
  ExpectDroppedId(media::VideoCaptureBufferPool::kInvalidId);
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_lo, GetParam());
  void* const memory_pointer_lo = buffer2->data();
  ASSERT_EQ(memory_pointer_hi, memory_pointer_lo)
      << "Decrease in resolution should not reallocate buffer";
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(3, buffer2->id());
  ASSERT_LE(format_lo.ImageAllocationSize(), buffer2->mapped_size());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, GetParam())) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Tear down the pool_, writing into the buffers. The buffer should preserve
  // the lifetime of the underlying memory.
  buffer3.reset();
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  pool_ = nullptr;

  // Touch the memory.
  if (buffer2->data() != nullptr)
    memset(buffer2->data(), 0x22, buffer2->mapped_size());
  if (buffer4->data() != nullptr)
    memset(buffer4->data(), 0x55, buffer4->mapped_size());
  buffer2.reset();

  if (buffer4->data() != nullptr)
    memset(buffer4->data(), 0x77, buffer4->mapped_size());
  buffer4.reset();
}

INSTANTIATE_TEST_CASE_P(,
                        VideoCaptureBufferPoolTest,
                        testing::ValuesIn(kCapturePixelFormats));

} // namespace content

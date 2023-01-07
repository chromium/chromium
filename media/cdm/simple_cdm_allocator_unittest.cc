// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>

#include "media/base/video_frame.h"
#include "media/cdm/api/content_decryption_module.h"
#include "media/cdm/cdm_helpers.h"
#include "media/cdm/simple_cdm_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class TestCdmBuffer final : public cdm::Buffer {
 public:
  static TestCdmBuffer* Create(uint32_t capacity) {
    return new TestCdmBuffer(capacity);
  }

  TestCdmBuffer(const TestCdmBuffer&) = delete;
  TestCdmBuffer& operator=(const TestCdmBuffer&) = delete;

  // cdm::Buffer implementation.
  void Destroy() override {
    DestroyCalled();
    delete this;
  }
  uint32_t Capacity() const override { return buffer_.size(); }
  uint8_t* Data() override { return buffer_.data(); }
  void SetSize(uint32_t size) override { size_ = size > Capacity() ? 0 : size; }
  uint32_t Size() const override { return size_; }

 private:
  TestCdmBuffer(uint32_t capacity) : buffer_(capacity), size_(0) {
    // Verify that Destroy() is called on this object.
    EXPECT_CALL(*this, DestroyCalled());
  }
  ~TestCdmBuffer() override = default;

  MOCK_METHOD0(DestroyCalled, void());

  std::vector<uint8_t> buffer_;
  uint32_t size_;
};

class SimpleCdmAllocatorTest : public testing::Test {
 public:
  SimpleCdmAllocatorTest() = default;

  SimpleCdmAllocatorTest(const SimpleCdmAllocatorTest&) = delete;
  SimpleCdmAllocatorTest& operator=(const SimpleCdmAllocatorTest&) = delete;

  ~SimpleCdmAllocatorTest() override = default;

 protected:
  SimpleCdmAllocator allocator_;
};

TEST_F(SimpleCdmAllocatorTest, CreateCdmBuffer) {
  cdm::Buffer* buffer = allocator_.CreateCdmBuffer(100);
  EXPECT_GE(buffer->Capacity(), 100u);
  buffer->Destroy();
}

TEST_F(SimpleCdmAllocatorTest, CreateCdmVideoFrame) {
  std::unique_ptr<VideoFrameImpl> video_frame =
      allocator_.CreateCdmVideoFrame();
  EXPECT_EQ(video_frame->FrameBuffer(), nullptr);
  video_frame->SetFrameBuffer(TestCdmBuffer::Create(100));
  EXPECT_NE(video_frame->FrameBuffer(), nullptr);

  // Releasing |video_frame| should free the cdm::Buffer created above
  // (verified by the mock method TestCdmBuffer::DestroyCalled).
  video_frame.reset();
}

TEST_F(SimpleCdmAllocatorTest, TransformToVideoFrame) {
  // For this test we need to pretend we have valid video data. So create
  // a small video frame of size 2x2.
  gfx::Size size(2, 2);
  size_t memory_needed = VideoFrame::AllocationSize(PIXEL_FORMAT_I420, size);

  // Now create a VideoFrameImpl.
  std::unique_ptr<VideoFrameImpl> video_frame =
      allocator_.CreateCdmVideoFrame();
  EXPECT_EQ(video_frame->FrameBuffer(), nullptr);

  // Fill VideoFrameImpl as if it was a small video frame.
  video_frame->SetFormat(cdm::kI420);
  video_frame->SetSize({size.width(), size.height()});
  video_frame->SetFrameBuffer(TestCdmBuffer::Create(memory_needed));
  video_frame->FrameBuffer()->SetSize(memory_needed);

  // Now transform VideoFrameImpl to a VideoFrame, and make sure that
  // FrameBuffer() is transferred to the new object.
  EXPECT_NE(video_frame->FrameBuffer(), nullptr);
  scoped_refptr<media::VideoFrame> transformed_frame =
      video_frame->TransformToVideoFrame(size);
  EXPECT_EQ(video_frame->FrameBuffer(), nullptr);

  // Releasing |transformed_frame| should free the cdm::Buffer created above
  // (verified by the mock method TestCdmBuffer::DestroyCalled).
  transformed_frame = nullptr;
}

}  // namespace media

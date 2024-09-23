// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/common/offset_byte_queue.h"

#include <stdint.h>
#include <string.h>

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class OffsetByteQueueTest : public testing::Test {
 public:
  void SetUp() override {
    uint8_t buf[256];
    for (int i = 0; i < 256; i++) {
      buf[i] = i;
    }
    queue_ = std::make_unique<OffsetByteQueue>();
    ASSERT_TRUE(queue_->Push(buf)) << "Test should not hit OOM";
    ASSERT_TRUE(queue_->Push(buf)) << "Test should not hit OOM";
    queue_->Pop(384);

    // Queue will start with 128 bytes of data and an offset of 384 bytes.
    // These values are used throughout the test.
  }

 protected:
  std::unique_ptr<OffsetByteQueue> queue_;
};

TEST_F(OffsetByteQueueTest, SetUp) {
  EXPECT_EQ(384, queue_->head());
  EXPECT_EQ(512, queue_->tail());

  const uint8_t* buf;
  int size;

  queue_->Peek(&buf, &size);
  EXPECT_EQ(128, size);
  EXPECT_EQ(128, buf[0]);
  EXPECT_EQ(255, buf[size-1]);
}

TEST_F(OffsetByteQueueTest, PeekAt) {
  const uint8_t* buf;
  int size;

  queue_->PeekAt(400, &buf, &size);
  EXPECT_EQ(queue_->tail() - 400, size);
  EXPECT_EQ(400 - 256, buf[0]);

  queue_->PeekAt(512, &buf, &size);
  EXPECT_EQ(NULL, buf);
  EXPECT_EQ(0, size);
}

TEST_F(OffsetByteQueueTest, Trim) {
  EXPECT_TRUE(queue_->Trim(128));
  EXPECT_TRUE(queue_->Trim(384));
  EXPECT_EQ(384, queue_->head());
  EXPECT_EQ(512, queue_->tail());

  EXPECT_TRUE(queue_->Trim(400));
  EXPECT_EQ(400, queue_->head());
  EXPECT_EQ(512, queue_->tail());

  const uint8_t* buf;
  int size;
  queue_->PeekAt(400, &buf, &size);
  EXPECT_EQ(queue_->tail() - 400, size);
  EXPECT_EQ(400 - 256, buf[0]);

  // Trimming to the exact end of the buffer should return 'true'. This
  // accommodates EOS cases.
  EXPECT_TRUE(queue_->Trim(512));
  EXPECT_EQ(512, queue_->head());
  queue_->Peek(&buf, &size);
  EXPECT_EQ(NULL, buf);

  // Trimming past the end of the buffer should return 'false'; we haven't seen
  // the preceding bytes.
  EXPECT_FALSE(queue_->Trim(513));

  // However, doing that shouldn't affect the EOS case. Only adding new data
  // should alter this behavior.
  EXPECT_TRUE(queue_->Trim(512));
}

}  // namespace media

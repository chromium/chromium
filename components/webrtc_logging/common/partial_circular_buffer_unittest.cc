// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test buffer data is 52 bytes, wrap position is set to 20 (this is
// arbitrarily chosen). The total buffer size is allocated dynamically based on
// the actual header size. This gives:
// Header of some size, non-wrapping part 20 bytes, wrapping part 32 bytes.
// As input data, a 14 byte array is used and repeatedly written. It's chosen
// not to be an integer factor smaller than the wrapping part. This ensures that
// the wrapped data isn't repeated at the same position.
// Note that desipte the number of wraps (if one or more), the reference output
// data is the same since the offset at each wrap is always the same.

#include "components/webrtc_logging/common/partial_circular_buffer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span_writer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webrtc_logging {

namespace {

const uint32_t kWrapPosition = 20;
const auto kInputData = std::to_array<const uint8_t>(
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14});
const auto kOutputRefDataWrap = std::to_array<const uint8_t>(
    {// The 20 bytes in the non-wrapping part.
     1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2, 3, 4, 5, 6,
     // The 32 bytes in wrapping part.
     11, 12, 13, 14, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 1, 2, 3, 4,
     5, 6, 7, 8, 9, 10, 11, 12, 13, 14});

}  // namespace

class PartialCircularBufferTest : public testing::Test {
 public:
  PartialCircularBufferTest() {
    buffer_header_size_ = offsetof(PartialCircularBuffer::BufferData, data);

    buffer_ = base::HeapArray<uint8_t>::Uninit(buffer_header_size_ +
                                               sizeof(kOutputRefDataWrap));
  }

  PartialCircularBufferTest(const PartialCircularBufferTest&) = delete;
  PartialCircularBufferTest& operator=(const PartialCircularBufferTest&) =
      delete;

  void InitWriteBuffer(bool append) {
    pcb_write_ =
        std::make_unique<PartialCircularBuffer>(buffer_, kWrapPosition, append);
  }

  void WriteToBuffer(int num) {
    for (int i = 0; i < num; ++i)
      pcb_write_->Write(kInputData);
  }

  void InitReadBuffer() {
    pcb_read_ = std::make_unique<PartialCircularBuffer>(buffer_);
  }

 protected:
  std::unique_ptr<PartialCircularBuffer> pcb_write_;
  std::unique_ptr<PartialCircularBuffer> pcb_read_;
  base::HeapArray<uint8_t> buffer_;
  uint32_t buffer_header_size_;
};

TEST_F(PartialCircularBufferTest, NoWrapBeginningPartOnly) {
  InitWriteBuffer(false);
  WriteToBuffer(1);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kInputData)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kInputData, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, NoWrapBeginningAndEndParts) {
  InitWriteBuffer(false);
  WriteToBuffer(2);
  InitReadBuffer();

  uint8_t output_data[2 * sizeof(kInputData)] = {};
  EXPECT_EQ(sizeof(output_data), pcb_read_->Read(output_data));

  const uint8_t output_ref_data[2 * sizeof(kInputData)] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  UNSAFE_TODO(
      EXPECT_EQ(0, memcmp(output_ref_data, output_data, sizeof(output_data))));

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapOnce) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapTwice) {
  InitWriteBuffer(false);
  WriteToBuffer(7);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapOnceSmallerOutputBuffer) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  const uint32_t size_per_read = 16;
  uint32_t read = 0;
  for (; read + size_per_read <= output_data.size(); read += size_per_read) {
    EXPECT_EQ(
        size_per_read,
        pcb_read_->Read(base::span(output_data).subspan(read, size_per_read)));
  }
  EXPECT_EQ(output_data.size() - read,
            pcb_read_->Read(base::span(output_data).subspan(read)));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapOnceWithAppend) {
  InitWriteBuffer(false);
  WriteToBuffer(2);
  InitWriteBuffer(true);
  WriteToBuffer(2);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapTwiceWithAppend) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitWriteBuffer(true);
  WriteToBuffer(3);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapOnceThenOverwriteWithNoWrap) {
  InitWriteBuffer(false);
  WriteToBuffer(4);
  InitWriteBuffer(false);
  WriteToBuffer(1);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kInputData)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kInputData, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

TEST_F(PartialCircularBufferTest, WrapTwiceWithSingleWrite) {
  const size_t kInputSize = sizeof(kInputData);
  const size_t kLargeSize = kInputSize * 7;
  std::array<uint8_t, kLargeSize> large_input = {};
  base::SpanWriter<uint8_t> large_input_writer(large_input);
  for (size_t offset = 0; offset < kLargeSize; offset += kInputSize)
    EXPECT_TRUE(large_input_writer.Write(kInputData));

  InitWriteBuffer(false);
  pcb_write_->Write(large_input);
  InitReadBuffer();

  std::array<uint8_t, sizeof(kOutputRefDataWrap)> output_data = {};
  EXPECT_EQ(output_data.size(), pcb_read_->Read(output_data));

  EXPECT_EQ(kOutputRefDataWrap, output_data);

  EXPECT_EQ(0u, pcb_read_->Read(output_data));
}

}  // namespace webrtc_logging

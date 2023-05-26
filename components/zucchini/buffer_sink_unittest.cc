// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/buffer_sink.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

constexpr uint8_t kUninit = 0xFF;

class BufferSinkTest : public testing::Test {
 protected:
  BufferSinkTest()
      : buffer_(10, kUninit), sink_(buffer_.data(), buffer_.size()) {}

  std::vector<uint8_t> buffer_;
  BufferSink sink_;
};

TEST_F(BufferSinkTest, PutValue) {
  EXPECT_EQ(size_t(10), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint32_t(0x76543210)));
  EXPECT_EQ(size_t(6), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint32_t(0xFEDCBA98)));
  EXPECT_EQ(size_t(2), sink_.Remaining());

  EXPECT_FALSE(sink_.PutValue(uint32_t(0x00)));
  EXPECT_EQ(size_t(2), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint16_t(0x0010)));
  EXPECT_EQ(size_t(0), sink_.Remaining());

  // Assuming little-endian architecture.
  EXPECT_EQ(std::vector<uint8_t>(
                {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x00}),
            buffer_);
}

TEST_F(BufferSinkTest, PutValueUnaligned) {
  EXPECT_EQ(size_t(10), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint8_t(0x10U)));
  EXPECT_EQ(size_t(9), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint16_t(0x5432U)));
  EXPECT_EQ(size_t(7), sink_.Remaining());

  EXPECT_TRUE(sink_.PutValue(uint32_t(0xDCBA9876U)));
  EXPECT_EQ(size_t(3), sink_.Remaining());
}

TEST_F(BufferSinkTest, PutRange) {
  std::vector<uint8_t> range = {0x10, 0x32, 0x54, 0x76, 0x98, 0xBA,
                                0xDC, 0xFE, 0x10, 0x00, 0x42};

  EXPECT_EQ(size_t(10), sink_.Remaining());
  EXPECT_FALSE(sink_.PutRange(range.begin(), range.end()));
  EXPECT_EQ(size_t(10), sink_.Remaining());

  EXPECT_TRUE(sink_.PutRange(range.begin(), range.begin() + 8));
  EXPECT_EQ(size_t(2), sink_.Remaining());
  EXPECT_EQ(std::vector<uint8_t>({0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC,
                                  0xFE, kUninit, kUninit}),
            buffer_);

  EXPECT_FALSE(sink_.PutRange(range.begin(), range.begin() + 4));
  EXPECT_EQ(size_t(2), sink_.Remaining());

  // range is not written
  EXPECT_EQ(std::vector<uint8_t>({0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC,
                                  0xFE, kUninit, kUninit}),
            buffer_);
}

}  // namespace zucchini

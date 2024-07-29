// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/scoped_d3d_buffers.h"

#include "base/containers/heap_array.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

class MemoryBuffer : public ScopedD3DBuffer {
 public:
  explicit MemoryBuffer(size_t size)
      : raw_data_(base::HeapArray<uint8_t>::Uninit(size)) {
    data_ = raw_data_.as_span();
  }

  bool Commit() override {
    raw_data_ = base::HeapArray<uint8_t>();
    return true;
  }

  bool Commit(uint32_t) override { return Commit(); }

 private:
  base::HeapArray<uint8_t> raw_data_;
};

}  // namespace

class ScopedD3DBufferTest : public ::testing::Test {
 protected:
  const size_t size_ = 15;
};

TEST_F(ScopedD3DBufferTest, D3DInputBuffer) {
  D3DInputBuffer input_buffer(
      std::unique_ptr<ScopedD3DBuffer>(new MemoryBuffer(size_)));
  EXPECT_FALSE(input_buffer.empty());
  EXPECT_EQ(input_buffer.size(), size_);
  EXPECT_TRUE(input_buffer.Commit());
}

TEST_F(ScopedD3DBufferTest, ScopedRandomAccessD3DInputBuffer) {
  ScopedRandomAccessD3DInputBuffer buffer(
      std::unique_ptr<ScopedD3DBuffer>(new MemoryBuffer(size_)));
  EXPECT_NE(buffer.data(), nullptr);
  EXPECT_TRUE(buffer.Commit());
}

TEST_F(ScopedD3DBufferTest, ScopedSequenceD3DInputBuffer) {
  ScopedSequenceD3DInputBuffer buffer(
      std::unique_ptr<ScopedD3DBuffer>(new MemoryBuffer(size_)));
  EXPECT_EQ(buffer.BytesWritten(), 0ull);
  EXPECT_EQ(buffer.BytesAvailable(), size_);

  uint8_t data[] = {0, 0, 1};
  EXPECT_EQ(buffer.Write(data), sizeof(data));
  EXPECT_EQ(buffer.BytesWritten(), sizeof(data));
  EXPECT_EQ(buffer.BytesAvailable(), size_ - sizeof(data));
  EXPECT_TRUE(buffer.Commit());
}

TEST_F(ScopedD3DBufferTest, FillUpScopedSequenceD3DInputBuffer) {
  ScopedSequenceD3DInputBuffer buffer(
      std::unique_ptr<ScopedD3DBuffer>(new MemoryBuffer(size_)));
  size_t larger_size = size_ + 10;
  std::unique_ptr<uint8_t[]> data(new uint8_t[larger_size]);
  EXPECT_EQ(buffer.Write({data.get(), larger_size}), size_);
  EXPECT_EQ(buffer.BytesWritten(), size_);
  EXPECT_EQ(buffer.BytesAvailable(), 0ull);
  EXPECT_TRUE(buffer.Commit());
}

}  // namespace media

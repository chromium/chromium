// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/entry_write_buffer.h"

#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace disk_cache {

TEST(EntryWriteBufferTest, DefaultConstructor) {
  EntryWriteBuffer buffer;
  EXPECT_TRUE(buffer.buffers.empty());
  EXPECT_EQ(buffer.size, 0);
  EXPECT_EQ(buffer.offset, 0);
}

TEST(EntryWriteBufferTest, SingleBufferConstructor) {
  const std::string kData = "test data";
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EntryWriteBuffer buffer(io_buffer, kData.size(), 100);

  EXPECT_EQ(buffer.buffers.size(), 1u);
  EXPECT_EQ(buffer.buffers[0], io_buffer);
  EXPECT_EQ(buffer.size, static_cast<int>(kData.size()));
  EXPECT_EQ(buffer.offset, 100);
}

TEST(EntryWriteBufferTest, SingleBufferConstructorNull) {
  EntryWriteBuffer buffer(nullptr, 0, 100);

  EXPECT_TRUE(buffer.buffers.empty());
  EXPECT_EQ(buffer.size, 0);
  EXPECT_EQ(buffer.offset, 100);
}

TEST(EntryWriteBufferTest, MoveConstructor) {
  const std::string kData = "test data";
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EntryWriteBuffer buffer1(io_buffer, kData.size(), 100);

  EntryWriteBuffer buffer2(std::move(buffer1));

  EXPECT_EQ(buffer2.buffers.size(), 1u);
  EXPECT_EQ(buffer2.buffers[0], io_buffer);
  EXPECT_EQ(buffer2.size, static_cast<int>(kData.size()));
  EXPECT_EQ(buffer2.offset, 100);

  // Moved-from object state.
  EXPECT_TRUE(buffer1.buffers.empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.size, 0);            // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.offset, 0);          // NOLINT(bugprone-use-after-move)
}

TEST(EntryWriteBufferTest, MoveAssignment) {
  const std::string kData = "test data";
  auto io_buffer = base::MakeRefCounted<net::StringIOBuffer>(kData);
  EntryWriteBuffer buffer1(io_buffer, kData.size(), 100);
  EntryWriteBuffer buffer2;

  buffer2 = std::move(buffer1);

  EXPECT_EQ(buffer2.buffers.size(), 1u);
  EXPECT_EQ(buffer2.buffers[0], io_buffer);
  EXPECT_EQ(buffer2.size, static_cast<int>(kData.size()));
  EXPECT_EQ(buffer2.offset, 100);

  // Moved-from object state.
  EXPECT_TRUE(buffer1.buffers.empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.size, 0);            // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer1.offset, 0);          // NOLINT(bugprone-use-after-move)
}

TEST(EntryWriteBufferTest, MultipleBuffers) {
  EntryWriteBuffer buffer;
  const std::string kData1 = "data1";
  const std::string kData2 = "data2";
  auto io_buffer1 = base::MakeRefCounted<net::StringIOBuffer>(kData1);
  auto io_buffer2 = base::MakeRefCounted<net::StringIOBuffer>(kData2);

  buffer.buffers.push_back(io_buffer1);
  buffer.buffers.push_back(io_buffer2);
  buffer.size = kData1.size() + kData2.size();
  buffer.offset = 100;

  EXPECT_EQ(buffer.buffers.size(), 2u);
  EXPECT_EQ(buffer.buffers[0], io_buffer1);
  EXPECT_EQ(buffer.buffers[1], io_buffer2);
  EXPECT_EQ(buffer.size, static_cast<int>(kData1.size() + kData2.size()));
  EXPECT_EQ(buffer.offset, 100);

  // Test move constructor with multiple buffers
  EntryWriteBuffer buffer2(std::move(buffer));
  EXPECT_EQ(buffer2.buffers.size(), 2u);
  EXPECT_EQ(buffer2.buffers[0], io_buffer1);
  EXPECT_EQ(buffer2.buffers[1], io_buffer2);
  EXPECT_EQ(buffer2.size, static_cast<int>(kData1.size() + kData2.size()));
  EXPECT_EQ(buffer2.offset, 100);

  EXPECT_TRUE(buffer.buffers.empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer.size, 0);            // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer.offset, 0);          // NOLINT(bugprone-use-after-move)

  // Test move assignment with multiple buffers
  EntryWriteBuffer buffer3;
  buffer3 = std::move(buffer2);
  EXPECT_EQ(buffer3.buffers.size(), 2u);
  EXPECT_EQ(buffer3.buffers[0], io_buffer1);
  EXPECT_EQ(buffer3.buffers[1], io_buffer2);
  EXPECT_EQ(buffer3.size, static_cast<int>(kData1.size() + kData2.size()));
  EXPECT_EQ(buffer3.offset, 100);

  EXPECT_TRUE(buffer2.buffers.empty());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer2.size, 0);            // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(buffer2.offset, 0);          // NOLINT(bugprone-use-after-move)
}

}  // namespace disk_cache

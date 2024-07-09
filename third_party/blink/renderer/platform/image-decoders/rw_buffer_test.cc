// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/rw_buffer.h"

#include <array>

#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {
namespace {

const char gABC[] = "abcdefghijklmnopqrstuvwxyz";

void check_abcs(const char buffer[], size_t size) {
  ASSERT_EQ(size % 26, 0u);
  for (size_t offset = 0; offset < size; offset += 26) {
    EXPECT_TRUE(!memcmp(&buffer[offset], gABC, 26));
  }
}

// reader should contains an integral number of copies of gABC.
void check_alphabet_buffer(const ROBuffer* reader) {
  size_t size = reader->size();
  ASSERT_EQ(size % 26, 0u);

  std::vector<char> storage(size);
  ROBuffer::Iter iter(reader);
  size_t offset = 0;
  do {
    ASSERT_LE(offset + iter.size(), size);
    memcpy(storage.data() + offset, iter.data(), iter.size());
    offset += iter.size();
  } while (iter.Next());
  ASSERT_EQ(offset, size);
  check_abcs(storage.data(), size);
}

size_t write_into_buffer(size_t reps, void* buffer, size_t capacity) {
  size_t len = std::min(capacity, reps * sizeof(gABC));
  for (size_t i = 0; i < len; i += 26U) {
    memcpy(static_cast<char*>(buffer) + i, gABC,
           std::min<size_t>(26U, len - i));
  }
  return len;
}

class ROBufferTestThread : public base::PlatformThread::Delegate {
 public:
  ROBufferTestThread(scoped_refptr<ROBuffer> reader, size_t i)
      : reader_(reader), i_(i) {}
  ROBufferTestThread() = default;
  ROBufferTestThread(const ROBufferTestThread&) = default;
  ROBufferTestThread& operator=(const ROBufferTestThread&) = default;

  void ThreadMain() override {
    EXPECT_EQ((i_ + 1) * 26U, reader_->size());
    check_alphabet_buffer(reader_.get());
  }

  scoped_refptr<ROBuffer> reader_;
  size_t i_;
};

}  // namespace

TEST(RWBufferTest, Append) {
  // Knowing that the default capacity is 4096, choose N large enough so we
  // force it to use multiple buffers internally.
  static constexpr size_t N = 1000;
  std::array<scoped_refptr<ROBuffer>, N> readers;

  {
    RWBuffer buffer;
    for (size_t i = 0; i < N; ++i) {
      buffer.Append(gABC, 26);
      readers[i] = buffer.MakeROBufferSnapshot();
    }
    EXPECT_EQ(N * 26, buffer.size());
  }

  // Verify that although the RWBuffer's destructor has run, the readers are
  // still valid.
  for (size_t i = 0; i < N; ++i) {
    EXPECT_EQ((i + 1) * 26U, readers[i]->size());
    check_alphabet_buffer(readers[i].get());
  }
}

TEST(RWBufferTest, Threaded) {
  // Knowing that the default capacity is 4096, choose N large enough so we
  // force it to use multiple buffers internally.
  constexpr size_t N = 1000;
  RWBuffer buffer;
  std::array<ROBufferTestThread, N> threads;
  std::array<base::PlatformThreadHandle, N> handlers;

  for (size_t i = 0; i < N; ++i) {
    buffer.Append(gABC, 26);
    scoped_refptr<ROBuffer> reader = buffer.MakeROBufferSnapshot();
    EXPECT_EQ(reader->size(), buffer.size());

    // reader's copy constructor will ref the ROBuffer, which will be unreffed
    // when the task ends.
    // Ownership of stream is passed to the task, which will delete it.
    threads[i] = ROBufferTestThread(reader, i);
    ASSERT_TRUE(base::PlatformThread::Create(0, &threads[i], &handlers[i]));
  }
  EXPECT_EQ(N * 26, buffer.size());
  for (size_t i = 0; i < N; ++i) {
    base::PlatformThread::Join(handlers[i]);
  }
}

// Tests that it is safe to call ROBuffer::Iter::size() when exhausted.
TEST(RWBufferTest, Size) {
  RWBuffer buffer;
  buffer.Append(gABC, 26);

  scoped_refptr<ROBuffer> roBuffer(buffer.MakeROBufferSnapshot());
  ROBuffer::Iter iter(roBuffer.get());
  EXPECT_TRUE(iter.data());
  EXPECT_EQ(iter.size(), 26u);

  // There is only one block in this buffer.
  EXPECT_TRUE(!iter.Next());
  EXPECT_EQ(0u, iter.size());
}

// Tests that operations (including the destructor) are safe on an RWBuffer
// without any data appended.
TEST(RWBufferTest, Empty) {
  RWBuffer buffer;
  ASSERT_EQ(0u, buffer.size());

  scoped_refptr<ROBuffer> roBuffer = buffer.MakeROBufferSnapshot();
  ASSERT_TRUE(roBuffer);
  if (roBuffer) {
    EXPECT_EQ(roBuffer->size(), 0u);
    ROBuffer::Iter iter(roBuffer.get());
    EXPECT_EQ(iter.size(), 0u);
    EXPECT_TRUE(!iter.data());
    EXPECT_TRUE(!iter.Next());
  }
}

// Tests that |HasNoSnapshots| returns the correct value when the buffer is
// empty.
// In this case, we can't tell if a snapshot has been created (in general), so
// we expect to always get back false.
TEST(RWBufferTest, HasNoSnapshotsEmpty) {
  RWBuffer buffer;
  ASSERT_EQ(0u, buffer.size());

  EXPECT_TRUE(buffer.HasNoSnapshots());

  {
    scoped_refptr<ROBuffer> first = buffer.MakeROBufferSnapshot();
    EXPECT_TRUE(buffer.HasNoSnapshots());

    scoped_refptr<ROBuffer> second = buffer.MakeROBufferSnapshot();
    EXPECT_TRUE(buffer.HasNoSnapshots());
  }

  EXPECT_TRUE(buffer.HasNoSnapshots());
}

// Tests that |HasNoSnapshots| returns the correct value when the buffer is
// non-empty.
TEST(RWBufferTest, HasNoSnapshots) {
  RWBuffer buffer;
  ASSERT_EQ(0u, buffer.size());

  buffer.Append(gABC, 26);

  EXPECT_TRUE(buffer.HasNoSnapshots());

  {
    {
      scoped_refptr<ROBuffer> first = buffer.MakeROBufferSnapshot();
      EXPECT_FALSE(buffer.HasNoSnapshots());
    }

    scoped_refptr<ROBuffer> second = buffer.MakeROBufferSnapshot();
    EXPECT_FALSE(buffer.HasNoSnapshots());
  }

  EXPECT_TRUE(buffer.HasNoSnapshots());
}

TEST(RWBufferTest, FunctionConstructorSmall) {
  RWBuffer buffer(base::BindOnce(&write_into_buffer, 1), 20);

  EXPECT_EQ(20U, buffer.size());

  scoped_refptr<ROBuffer> roBuffer = buffer.MakeROBufferSnapshot();
  ROBuffer::Iter iter(roBuffer.get());
  EXPECT_EQ(0, memcmp(iter.data(), gABC, 20U));
}

TEST(RWBufferTest, FunctionConstructorLarge) {
  RWBuffer buffer(base::BindOnce(&write_into_buffer, 1000), 1000 * 26);

  EXPECT_EQ(1000U * 26, buffer.size());

  auto ro_buffer = buffer.MakeROBufferSnapshot();
  check_alphabet_buffer(ro_buffer.get());
}

}  // namespace blink

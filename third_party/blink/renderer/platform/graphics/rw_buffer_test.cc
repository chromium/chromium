// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/rw_buffer.h"

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

// stream should contain an integral number of copies of gABC.
void check_alphabet_stream(SkStream* stream) {
  ASSERT_TRUE(stream->hasLength());
  size_t size = stream->getLength();
  ASSERT_EQ(size % 26, 0u);

  std::vector<char> storage(size);
  char* array = storage.data();
  size_t bytesRead = stream->read(array, size);
  EXPECT_EQ(bytesRead, size);
  check_abcs(array, size);

  // try checking backwards
  for (size_t offset = size; offset > 0; offset -= 26) {
    EXPECT_TRUE(stream->seek(offset - 26));
    EXPECT_EQ(stream->getPosition(), offset - 26);
    EXPECT_EQ(stream->read(array, 26), 26u);
    check_abcs(array, 26);
    EXPECT_EQ(stream->getPosition(), offset);
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

class ROBufferTestThread : public base::PlatformThread::Delegate {
 public:
  ROBufferTestThread(sk_sp<ROBuffer> reader, SkStream* stream, size_t i)
      : reader_(reader), stream_(stream), i_(i) {}
  ROBufferTestThread() = default;
  ROBufferTestThread(const ROBufferTestThread&) = default;

  void ThreadMain() override {
    EXPECT_EQ((i_ + 1) * 26U, reader_->size());
    EXPECT_EQ(stream_->getLength(), reader_->size());
    check_alphabet_buffer(reader_.get());
    check_alphabet_stream(stream_);
    EXPECT_TRUE(stream_->rewind());
    delete stream_;
  }

  sk_sp<ROBuffer> reader_;
  SkStream* stream_;
  size_t i_;
};

}  // namespace

TEST(RWBuffer, reporter) {
  // Knowing that the default capacity is 4096, choose N large enough so we
  // force it to use multiple buffers internally.
  static constexpr size_t N = 1000;
  std::array<sk_sp<ROBuffer>, N> readers;
  std::array<std::unique_ptr<SkStream>, N> streams;

  {
    RWBuffer buffer;
    for (size_t i = 0; i < N; ++i) {
      buffer.Append(gABC, 26);
      readers[i] = buffer.MakeROBufferSnapshot();
      streams[i] = buffer.MakeStreamSnapshot();
    }
    EXPECT_EQ(N * 26, buffer.size());
  }

  // Verify that although the RWBuffer's destructor has run, the readers are
  // still valid.
  for (size_t i = 0; i < N; ++i) {
    EXPECT_EQ((i + 1) * 26U, readers[i]->size());
    check_alphabet_buffer(readers[i].get());
    check_alphabet_stream(streams[i].get());
  }
}

TEST(RWBuffer_threaded, reporter) {
  // Knowing that the default capacity is 4096, choose N large enough so we
  // force it to use multiple buffers internally.
  constexpr size_t N = 1000;
  RWBuffer buffer;
  std::array<ROBufferTestThread, N> threads;
  std::array<base::PlatformThreadHandle, N> handlers;

  for (size_t i = 0; i < N; ++i) {
    buffer.Append(gABC, 26);
    sk_sp<ROBuffer> reader = buffer.MakeROBufferSnapshot();
    SkStream* stream = buffer.MakeStreamSnapshot().release();
    EXPECT_EQ(reader->size(), buffer.size());
    EXPECT_EQ(stream->getLength(), buffer.size());

    // reader's copy constructor will ref the ROBuffer, which will be unreffed
    // when the task ends.
    // Ownership of stream is passed to the task, which will delete it.
    threads[i] = ROBufferTestThread(reader, stream, i);
    ASSERT_TRUE(base::PlatformThread::Create(0, &threads[i], &handlers[i]));
  }
  EXPECT_EQ(N * 26, buffer.size());
  for (size_t i = 0; i < N; ++i) {
    base::PlatformThread::Join(handlers[i]);
  }
}

// Tests that it is safe to call ROBuffer::Iter::size() when exhausted.
TEST(RWBuffer_size, r) {
  RWBuffer buffer;
  buffer.Append(gABC, 26);

  sk_sp<ROBuffer> roBuffer(buffer.MakeROBufferSnapshot());
  ROBuffer::Iter iter(roBuffer.get());
  EXPECT_TRUE(iter.data());
  EXPECT_EQ(iter.size(), 26u);

  // There is only one block in this buffer.
  EXPECT_TRUE(!iter.Next());
  EXPECT_EQ(0u, iter.size());
}

// Tests that operations (including the destructor) are safe on an RWBuffer
// without any data appended.
TEST(RWBuffer_noAppend, r) {
  RWBuffer buffer;
  ASSERT_EQ(0u, buffer.size());

  sk_sp<ROBuffer> roBuffer = buffer.MakeROBufferSnapshot();
  ASSERT_TRUE(roBuffer);
  if (roBuffer) {
    EXPECT_EQ(roBuffer->size(), 0u);
    ROBuffer::Iter iter(roBuffer.get());
    EXPECT_EQ(iter.size(), 0u);
    EXPECT_TRUE(!iter.data());
    EXPECT_TRUE(!iter.Next());
  }

  std::unique_ptr<SkStream> stream(buffer.MakeStreamSnapshot());
  EXPECT_TRUE(stream);
  if (stream) {
    EXPECT_TRUE(stream->hasLength());
    EXPECT_EQ(stream->getLength(), 0u);
    EXPECT_EQ(stream->skip(10), 0u);
  }
}

}  // namespace blink

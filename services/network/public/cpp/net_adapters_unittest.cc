// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_adapters.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(NetAdaptersTest, StagedWriteCommitsOnComplete) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(4096, producer, consumer));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  ASSERT_TRUE(pending);

  constexpr std::string_view kExpected = "SamplePayload";
  base::as_writable_byte_span(*pending)
      .first(kExpected.size())
      .copy_from(base::as_byte_span(kExpected));

  // Staged bytes are not readable from the pipe until `Complete()` runs.
  std::string actual(kExpected.size(), '\0');
  size_t bytes_read = 0;
  EXPECT_EQ(
      MOJO_RESULT_SHOULD_WAIT,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(actual), bytes_read));

  producer = pending->Complete(kExpected.size());
  pending.reset();

  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(actual), bytes_read));
  EXPECT_EQ(kExpected.size(), bytes_read);
  EXPECT_EQ(kExpected, actual);
}

TEST(NetAdaptersTest, StagingBufferRecycledWhenUniquelyOwned) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(65536, producer, consumer));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  char* first_staging_ptr = pending->buffer();
  {
    auto io_buf = base::MakeRefCounted<NetToMojoIOBuffer>(pending);
    io_buf->span().first(4u).copy_from(base::byte_span_from_cstring("abcd"));
  }
  producer = pending->Complete(4);
  pending.reset();
  EXPECT_TRUE(pool.HasBufferForTesting());

  // Drain the 4 bytes so the full 65536-byte capacity is available again.
  std::string drained(4, '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(drained), bytes_read));
  ASSERT_EQ(4u, bytes_read);

  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  EXPECT_FALSE(pool.HasBufferForTesting());
  EXPECT_EQ(first_staging_ptr, pending->buffer());
  producer = pending->Complete(0);
}

TEST(NetAdaptersTest,
     StagingBufferPoolGrowsWhenSubsequentWriteNeedsLargerBuffer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(4096, producer, consumer));

  // Fill 4080 of 4096 bytes so the initial staged write only sees 16 bytes of
  // writable pipe capacity (`buf.size() == 16`).
  std::string initial_fill(4080, 'a');
  size_t written = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            producer->WriteData(base::as_byte_span(initial_fill),
                                MOJO_WRITE_DATA_FLAG_ALL_OR_NONE, written));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  ASSERT_EQ(16u, pending->size());
  std::ranges::fill(base::as_writable_byte_span(*pending), uint8_t{'b'});
  producer = pending->Complete(16);
  pending.reset();
  EXPECT_TRUE(pool.HasBufferForTesting());

  // Drain the entire 4096-byte pipe so the next `BeginWrite()` sees `buf.size()
  // == 4096`, replacing the 16-byte cached buffer with a 4096-byte buffer.
  std::string drained(4096, '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_ALL_OR_NONE,
                         base::as_writable_byte_span(drained), bytes_read));

  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  ASSERT_EQ(4096u, pending->size());
  char* grown_ptr = pending->buffer();
  producer = pending->Complete(0);
  pending.reset();
  EXPECT_TRUE(pool.HasBufferForTesting());

  // Subsequent writes reuse the 4096-byte buffer.
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  EXPECT_EQ(grown_ptr, pending->buffer());
  producer = pending->Complete(0);
}

TEST(NetAdaptersTest, CompleteAfterPoolDestroyedSafelyReleasesStagingBuffer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(4096, producer, consumer));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  auto pool = std::make_optional<NetToMojoPendingBuffer::StagingBufferPool>();
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &*pool));
  base::as_writable_byte_span(*pending).first(4u).copy_from(
      base::byte_span_from_cstring("safe"));

  // Destroy the pool before `Complete()` runs; `WeakPtr` invalidation ensures
  // `Complete()` still commits bytes and frees `StagingBuffer` cleanly.
  pool.reset();
  producer = pending->Complete(4);
  pending.reset();

  std::string actual(4, '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(actual), bytes_read));
  EXPECT_EQ("safe", actual);
}

TEST(NetAdaptersTest,
     StagingBufferRetainedSafelyWhenIOBufferOutlivesCompleteOrDestruction) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(65536, producer, consumer));

  constexpr std::string_view kCachedChunk = "DiskCachePayload";
  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));

  // Simulate a background `disk_cache` thread holding `io_buf` past
  // `Complete()`.
  auto io_buf = base::MakeRefCounted<NetToMojoIOBuffer>(pending);
  io_buf->span()
      .first(kCachedChunk.size())
      .copy_from(base::as_byte_span(kCachedChunk));

  producer = pending->Complete(kCachedChunk.size());
  pending.reset();

  // Must not recycle `StagingBuffer` while `io_buf` still holds a reference.
  EXPECT_FALSE(pool.HasBufferForTesting());

  // Subsequent write allocates a distinct `StagingBuffer` without overwriting
  // `io_buf`.
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  EXPECT_NE(io_buf->data(), pending->buffer());
  std::ranges::fill(
      base::as_writable_byte_span(*pending).first(kCachedChunk.size()),
      uint8_t{'Z'});
  EXPECT_EQ(kCachedChunk,
            base::as_string_view(io_buf->span().first(kCachedChunk.size())));

  // Also verify dropping `pending` without calling `Complete()` immediately
  // ends the two-phase write with 0 bytes and closes the producer pipe while
  // keeping an outstanding `NetToMojoIOBuffer` valid.
  auto aborted_io_buf = base::MakeRefCounted<NetToMojoIOBuffer>(pending);
  aborted_io_buf->span().first(5u).copy_from(
      base::byte_span_from_cstring("abort"));
  pending.reset();
  EXPECT_EQ("abort", base::as_string_view(aborted_io_buf->span().first(5u)));

  // Only `kCachedChunk` should have been written, and the pipe should already
  // be closed while `aborted_io_buf` and `io_buf` are still alive.
  std::string pipe_contents(64, '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(MOJO_RESULT_OK,
            consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                               base::as_writable_byte_span(pipe_contents),
                               bytes_read));
  EXPECT_EQ(kCachedChunk.size(), bytes_read);
  EXPECT_EQ(kCachedChunk,
            std::string_view(pipe_contents).substr(0, bytes_read));
  EXPECT_EQ(MOJO_RESULT_FAILED_PRECONDITION,
            consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                               base::as_writable_byte_span(pipe_contents),
                               bytes_read));

  aborted_io_buf.reset();
  io_buf.reset();
}

TEST(NetAdaptersTest, DisabledFeatureWritesDirectly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(4096, producer, consumer));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  ASSERT_TRUE(pending);

  constexpr std::string_view kData = "DirectWritePayload";
  base::as_writable_byte_span(*pending)
      .first(kData.size())
      .copy_from(base::as_byte_span(kData));
  producer = pending->Complete(kData.size());
  pending.reset();
  EXPECT_FALSE(pool.HasBufferForTesting());

  std::string actual(kData.size(), '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(actual), bytes_read));
  EXPECT_EQ(kData, actual);
}

TEST(NetAdaptersTest, ShouldWaitPreservesRecycledStagingBuffer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kStageNetToMojoBuffer);

  mojo::ScopedDataPipeProducerHandle producer;
  mojo::ScopedDataPipeConsumerHandle consumer;
  ASSERT_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(16, producer, consumer));

  scoped_refptr<NetToMojoPendingBuffer> pending;
  NetToMojoPendingBuffer::StagingBufferPool pool;
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  char* expected_staging_ptr = pending->buffer();
  std::ranges::fill(base::as_writable_byte_span(*pending).first(16u),
                    uint8_t{'X'});
  producer = pending->Complete(16);
  pending.reset();
  ASSERT_TRUE(pool.HasBufferForTesting());

  // Pipe is now full; `BeginWrite()` must fail with `MOJO_RESULT_SHOULD_WAIT`
  // without consuming `producer` or `pool`'s cached buffer.
  EXPECT_EQ(MOJO_RESULT_SHOULD_WAIT,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  EXPECT_EQ(nullptr, pending);
  EXPECT_TRUE(producer.is_valid());
  EXPECT_TRUE(pool.HasBufferForTesting());

  // Drain the pipe and verify `pool`'s cached buffer is still reused on the
  // next write.
  std::string drained(16, '\0');
  size_t bytes_read = 0;
  ASSERT_EQ(
      MOJO_RESULT_OK,
      consumer->ReadData(MOJO_READ_DATA_FLAG_NONE,
                         base::as_writable_byte_span(drained), bytes_read));
  ASSERT_EQ(16u, bytes_read);
  ASSERT_EQ(MOJO_RESULT_OK,
            NetToMojoPendingBuffer::BeginWrite(&producer, &pending, &pool));
  EXPECT_EQ(expected_staging_ptr, pending->buffer());
  producer = pending->Complete(0);
}

}  // namespace
}  // namespace network

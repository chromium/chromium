// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer_stream.h"

#include <stdint.h>

#include <array>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr uint32_t kTrackId = 7;
constexpr base::TimeDelta kDuration = base::Seconds(10);

scoped_refptr<DecoderBuffer> CreateBuffer(base::TimeDelta timestamp) {
  constexpr std::array<uint8_t, 16> kData = {};
  auto buffer = DecoderBuffer::CopyFrom(kData);
  buffer->set_timestamp(timestamp);
  return buffer;
}

}  // namespace

class SymphoniaDemuxerStreamTest : public testing::Test {
 public:
  SymphoniaDemuxerStreamTest()
      : stream_(kTrackId,
                TestAudioConfig::Normal(),
                kDuration,
                request_data_cb_.Get()) {}

 protected:
  using ReadFuture = base::test::TestFuture<DemuxerStream::Status,
                                            DemuxerStream::DecoderBufferVector>;

  base::test::TaskEnvironment task_environment_;
  testing::NiceMock<base::MockRepeatingClosure> request_data_cb_;
  SymphoniaDemuxerStream stream_;
};

TEST_F(SymphoniaDemuxerStreamTest, Properties) {
  EXPECT_EQ(stream_.type(), DemuxerStream::AUDIO);
  EXPECT_EQ(stream_.liveness(), StreamLiveness::kRecorded);
  EXPECT_EQ(stream_.track_id(), kTrackId);
  EXPECT_EQ(stream_.duration(), kDuration);
  EXPECT_TRUE(
      stream_.audio_decoder_config().Matches(TestAudioConfig::Normal()));
  EXPECT_FALSE(stream_.SupportsConfigChanges());
  EXPECT_TRUE(stream_.is_enabled());
}

TEST_F(SymphoniaDemuxerStreamTest, ReadReturnsQueuedBuffers) {
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(20)));

  ReadFuture future;
  stream_.Read(2, future.GetCallback());
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 2u);
  EXPECT_EQ(buffers[0]->timestamp(), base::Milliseconds(0));
  EXPECT_EQ(buffers[1]->timestamp(), base::Milliseconds(10));
}

TEST_F(SymphoniaDemuxerStreamTest, PendingReadIsSatisfiedByEnqueue) {
  EXPECT_CALL(request_data_cb_, Run()).Times(testing::AtLeast(1));
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  EXPECT_TRUE(stream_.has_pending_read());
  EXPECT_FALSE(future.IsReady());

  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(5)));
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_EQ(buffers[0]->timestamp(), base::Milliseconds(5));
  EXPECT_FALSE(stream_.has_pending_read());
}

TEST_F(SymphoniaDemuxerStreamTest, ReturnsEndOfStreamOnceDrained) {
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.SetEndOfStream();

  ReadFuture first;
  stream_.Read(1, first.GetCallback());
  EXPECT_FALSE(
      first.Get<DemuxerStream::DecoderBufferVector>()[0]->end_of_stream());

  ReadFuture second;
  stream_.Read(1, second.GetCallback());
  auto [status, buffers] = second.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_TRUE(buffers[0]->end_of_stream());
}

TEST_F(SymphoniaDemuxerStreamTest, EndOfStreamReadDoesNotRequestData) {
  stream_.SetEndOfStream();

  EXPECT_CALL(request_data_cb_, Run()).Times(0);
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_TRUE(buffers[0]->end_of_stream());
}

TEST_F(SymphoniaDemuxerStreamTest, DisabledStreamReturnsEndOfStream) {
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.SetEnabled(false);
  EXPECT_FALSE(stream_.is_enabled());
  EXPECT_EQ(stream_.GetMemoryUsage(), 0);

  // Buffers enqueued while disabled are dropped.
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));
  EXPECT_EQ(stream_.GetMemoryUsage(), 0);

  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_TRUE(buffers[0]->end_of_stream());
}

TEST_F(SymphoniaDemuxerStreamTest,
       DisablingSatisfiesPendingReadWithEndOfStream) {
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  ASSERT_TRUE(stream_.has_pending_read());

  stream_.SetEnabled(false);
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_TRUE(buffers[0]->end_of_stream());
}

TEST_F(SymphoniaDemuxerStreamTest, ReenabledStreamKeepsEndOfStream) {
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.SetEndOfStream();
  stream_.SetEnabled(false);
  stream_.SetEnabled(true);

  // The demuxer does not read past end of stream, so the stream must not wait
  // for more packets.
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(future.IsReady());
  auto [status, buffers] = future.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_TRUE(buffers[0]->end_of_stream());
}

TEST_F(SymphoniaDemuxerStreamTest, DisablingKeepsAbortedState) {
  stream_.Abort();
  stream_.SetEnabled(false);
  stream_.SetEnabled(true);

  // Reads stay aborted until `FlushBuffers()`, which runs on seek.
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);
}

TEST_F(SymphoniaDemuxerStreamTest, AbortAbortsReadsUntilFlushed) {
  ReadFuture pending;
  stream_.Read(1, pending.GetCallback());
  stream_.Abort();
  EXPECT_EQ(pending.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);

  ReadFuture after_abort;
  stream_.Read(1, after_abort.GetCallback());
  EXPECT_EQ(after_abort.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);

  stream_.FlushBuffers();
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  ReadFuture after_flush;
  stream_.Read(1, after_flush.GetCallback());
  EXPECT_EQ(after_flush.Get<DemuxerStream::Status>(), DemuxerStream::kOk);
}

TEST_F(SymphoniaDemuxerStreamTest, StopAbortsReadsPermanently) {
  stream_.Stop();
  stream_.FlushBuffers();
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));

  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  EXPECT_EQ(future.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);
}

TEST_F(SymphoniaDemuxerStreamTest, FlushBuffersClearsQueueAndEndOfStream) {
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.SetEndOfStream();
  EXPECT_GT(stream_.GetMemoryUsage(), 0);
  EXPECT_FALSE(stream_.HasAvailableCapacity());

  stream_.FlushBuffers();
  EXPECT_EQ(stream_.GetMemoryUsage(), 0);
  EXPECT_TRUE(stream_.HasAvailableCapacity());
}

TEST_F(SymphoniaDemuxerStreamTest, HasAvailableCapacityUntilQueueIsFull) {
  int enqueued = 0;
  while (stream_.HasAvailableCapacity()) {
    stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(enqueued)));
    ++enqueued;
    ASSERT_LT(enqueued, 1000);
  }
  EXPECT_GT(enqueued, 0);

  // Reading from the full queue frees up capacity.
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
  EXPECT_EQ(future.Get<DemuxerStream::Status>(), DemuxerStream::kOk);
  EXPECT_TRUE(stream_.HasAvailableCapacity());
}

TEST_F(SymphoniaDemuxerStreamTest, RequestsDataWhenReadCannotBeSatisfied) {
  EXPECT_CALL(request_data_cb_, Run()).Times(testing::AtLeast(1));
  ReadFuture future;
  stream_.Read(1, future.GetCallback());
}

TEST_F(SymphoniaDemuxerStreamTest, NoCapacityWhenDisabled) {
  ASSERT_TRUE(stream_.HasAvailableCapacity());
  stream_.SetEnabled(false);
  EXPECT_FALSE(stream_.HasAvailableCapacity());
  stream_.SetEnabled(true);
  EXPECT_TRUE(stream_.HasAvailableCapacity());
}

TEST_F(SymphoniaDemuxerStreamTest, NoCapacityWhenAbortedUntilFlushed) {
  stream_.Abort();
  EXPECT_FALSE(stream_.HasAvailableCapacity());
  stream_.FlushBuffers();
  EXPECT_TRUE(stream_.HasAvailableCapacity());
}

TEST_F(SymphoniaDemuxerStreamTest, NoCapacityWhenStopped) {
  stream_.Stop();
  EXPECT_FALSE(stream_.HasAvailableCapacity());
  stream_.FlushBuffers();
  EXPECT_FALSE(stream_.HasAvailableCapacity());
}

}  // namespace media

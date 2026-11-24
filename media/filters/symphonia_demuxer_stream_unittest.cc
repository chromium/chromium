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
#include "media/base/media_track.h"
#include "media/base/test_helpers.h"
#include "media/base/timestamp_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

constexpr uint32_t kTrackId = 7;
// Differs from `kTrackId`, since the demuxer numbers `MediaTrack`s itself.
constexpr char kMediaTrackId[] = "1";
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
                MediaTrack::Id(kMediaTrackId),
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
  EXPECT_EQ(stream_.media_track_id(), MediaTrack::Id(kMediaTrackId));
  EXPECT_EQ(stream_.duration(), kDuration);
  EXPECT_TRUE(
      stream_.audio_decoder_config().Matches(TestAudioConfig::Normal()));
  EXPECT_FALSE(stream_.SupportsConfigChanges());
  EXPECT_TRUE(stream_.is_enabled());
}

TEST_F(SymphoniaDemuxerStreamTest, EstimatesDurationFromSymphoniaPackets) {
  constexpr base::TimeDelta kPacketDuration = base::Milliseconds(20);
  constexpr base::TimeDelta kLeadingTrim = base::Milliseconds(5);
  EXPECT_EQ(stream_.estimated_duration(), kNoTimestamp);

  // An initial all-padding packet leaves `estimated_duration_` unset.
  auto initial_padding = CreateBuffer(-kPacketDuration - kLeadingTrim);
  initial_padding->set_duration(base::TimeDelta());
  initial_padding->set_discard_padding({kPacketDuration, base::TimeDelta()});
  stream_.EnqueueBuffer(initial_padding);
  EXPECT_EQ(stream_.estimated_duration(), kNoTimestamp);

  // Symphonia shifts leading-trimmed packets to `-kLeadingTrim` and reduces
  // `duration()` to the valid portion.
  auto first = CreateBuffer(-kLeadingTrim);
  first->set_duration(kPacketDuration - kLeadingTrim);
  first->set_discard_padding({kLeadingTrim, base::TimeDelta()});
  stream_.EnqueueBuffer(first);
  EXPECT_EQ(stream_.estimated_duration(), -kLeadingTrim);

  // A zero-duration packet without discard padding (e.g. a container like WebM
  // `SimpleBlock` that omits packet duration) is not padding and still updates
  // `estimated_duration_`.
  auto zero_dur_unpadded = CreateBuffer(kPacketDuration - kLeadingTrim);
  zero_dur_unpadded->set_duration(base::TimeDelta());
  stream_.EnqueueBuffer(zero_dur_unpadded);
  EXPECT_EQ(stream_.estimated_duration(), kPacketDuration - kLeadingTrim);

  // A decreasing timestamp (e.g. after a backward seek) does not lower the
  // high-water mark.
  auto earlier = CreateBuffer(base::TimeDelta());
  earlier->set_duration(kPacketDuration);
  stream_.EnqueueBuffer(earlier);
  EXPECT_EQ(stream_.estimated_duration(), kPacketDuration - kLeadingTrim);

  // Partially trimmed packets, even with >50% trim, still carry valid frames
  // and extend `estimated_duration_`.
  constexpr base::TimeDelta kHeavyEndTrim = base::Milliseconds(12);
  auto heavily_trimmed = CreateBuffer(2 * kPacketDuration - kLeadingTrim);
  heavily_trimmed->set_duration(kPacketDuration - kHeavyEndTrim);
  heavily_trimmed->set_discard_padding({base::TimeDelta(), kHeavyEndTrim});
  stream_.EnqueueBuffer(heavily_trimmed);
  EXPECT_EQ(stream_.estimated_duration(), 2 * kPacketDuration - kLeadingTrim);

  // A trailing all-padding packet (`duration() == 0` with positive discard
  // padding) doesn't extend the estimate.
  auto all_padding = CreateBuffer(3 * kPacketDuration - kLeadingTrim);
  all_padding->set_duration(base::TimeDelta());
  all_padding->set_discard_padding({base::TimeDelta(), kPacketDuration});
  stream_.EnqueueBuffer(all_padding);
  EXPECT_EQ(stream_.estimated_duration(), 2 * kPacketDuration - kLeadingTrim);
}

TEST_F(SymphoniaDemuxerStreamTest, SetLiveness) {
  stream_.set_liveness(StreamLiveness::kUnknown);
  EXPECT_EQ(stream_.liveness(), StreamLiveness::kUnknown);
}

TEST_F(SymphoniaDemuxerStreamTest, ChangeConfigAfterQueuedBuffers) {
  stream_.set_supports_config_changes(true);
  EXPECT_TRUE(stream_.SupportsConfigChanges());

  // One packet for the old config, then the config changes, then one packet
  // for the new config.
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  const AudioDecoderConfig normal = TestAudioConfig::Normal();
  const AudioDecoderConfig new_config(
      normal.codec(), normal.sample_format(),
      ChannelLayoutConfig::Guess(normal.channels()),
      normal.samples_per_second() * 2, std::vector<uint8_t>(),
      EncryptionScheme::kUnencrypted);
  ASSERT_FALSE(new_config.Matches(normal));
  stream_.ChangeConfig(new_config);
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));

  // The old config's packet is read first, even though more are requested.
  ReadFuture first;
  stream_.Read(2, first.GetCallback());
  auto [first_status, first_buffers] = first.Take();
  EXPECT_EQ(first_status, DemuxerStream::kOk);
  ASSERT_EQ(first_buffers.size(), 1u);
  EXPECT_EQ(first_buffers[0]->timestamp(), base::Milliseconds(0));
  EXPECT_TRUE(
      stream_.audio_decoder_config().Matches(TestAudioConfig::Normal()));

  // Then the config change is reported.
  ReadFuture second;
  stream_.Read(2, second.GetCallback());
  EXPECT_EQ(second.Get<DemuxerStream::Status>(), DemuxerStream::kConfigChanged);
  EXPECT_TRUE(stream_.audio_decoder_config().Matches(new_config));

  // Then the new config's packet.
  ReadFuture third;
  stream_.Read(2, third.GetCallback());
  auto [third_status, third_buffers] = third.Take();
  EXPECT_EQ(third_status, DemuxerStream::kOk);
  ASSERT_EQ(third_buffers.size(), 1u);
  EXPECT_EQ(third_buffers[0]->timestamp(), base::Milliseconds(10));
}

TEST_F(SymphoniaDemuxerStreamTest, ChangeConfigToSameConfigEmitsConfigChanged) {
  stream_.set_supports_config_changes(true);
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.ChangeConfig(TestAudioConfig::Normal());
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));

  // Even when the config matches, a chained stream boundary must emit
  // `kConfigChanged` so the audio decoder resets its state.
  ReadFuture first;
  stream_.Read(2, first.GetCallback());
  auto [first_status, first_buffers] = first.Take();
  EXPECT_EQ(first_status, DemuxerStream::kOk);
  ASSERT_EQ(first_buffers.size(), 1u);
  EXPECT_EQ(first_buffers[0]->timestamp(), base::Milliseconds(0));

  ReadFuture second;
  stream_.Read(2, second.GetCallback());
  EXPECT_EQ(second.Get<DemuxerStream::Status>(), DemuxerStream::kConfigChanged);

  ReadFuture third;
  stream_.Read(2, third.GetCallback());
  auto [third_status, third_buffers] = third.Take();
  EXPECT_EQ(third_status, DemuxerStream::kOk);
  ASSERT_EQ(third_buffers.size(), 1u);
  EXPECT_EQ(third_buffers[0]->timestamp(), base::Milliseconds(10));
}

TEST_F(SymphoniaDemuxerStreamTest, MultipleQueuedConfigChangesPreservePackets) {
  stream_.set_supports_config_changes(true);
  const AudioDecoderConfig normal = TestAudioConfig::Normal();
  const AudioDecoderConfig second_config(
      normal.codec(), normal.sample_format(),
      ChannelLayoutConfig::Guess(normal.channels()),
      normal.samples_per_second() * 2, std::vector<uint8_t>(),
      EncryptionScheme::kUnencrypted);
  const AudioDecoderConfig third_config(
      normal.codec(), normal.sample_format(),
      ChannelLayoutConfig::Guess(normal.channels()),
      normal.samples_per_second() * 4, std::vector<uint8_t>(),
      EncryptionScheme::kUnencrypted);

  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.ChangeConfig(second_config);
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));
  stream_.ChangeConfig(third_config);
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(20)));

  ReadFuture read1;
  stream_.Read(2, read1.GetCallback());
  ASSERT_EQ(read1.Get<DemuxerStream::DecoderBufferVector>().size(), 1u);

  ReadFuture change1;
  stream_.Read(2, change1.GetCallback());
  EXPECT_EQ(change1.Get<DemuxerStream::Status>(),
            DemuxerStream::kConfigChanged);
  EXPECT_TRUE(stream_.audio_decoder_config().Matches(second_config));

  ReadFuture read2;
  stream_.Read(2, read2.GetCallback());
  auto [status2, buffers2] = read2.Take();
  EXPECT_EQ(status2, DemuxerStream::kOk);
  ASSERT_EQ(buffers2.size(), 1u);
  EXPECT_EQ(buffers2[0]->timestamp(), base::Milliseconds(10));

  ReadFuture change2;
  stream_.Read(2, change2.GetCallback());
  EXPECT_EQ(change2.Get<DemuxerStream::Status>(),
            DemuxerStream::kConfigChanged);
  EXPECT_TRUE(stream_.audio_decoder_config().Matches(third_config));

  ReadFuture read3;
  stream_.Read(2, read3.GetCallback());
  auto [status3, buffers3] = read3.Take();
  EXPECT_EQ(status3, DemuxerStream::kOk);
  ASSERT_EQ(buffers3.size(), 1u);
  EXPECT_EQ(buffers3[0]->timestamp(), base::Milliseconds(20));
}

TEST_F(SymphoniaDemuxerStreamTest, FlushBuffersPreservesPendingConfigChange) {
  stream_.set_supports_config_changes(true);
  const AudioDecoderConfig normal = TestAudioConfig::Normal();
  const AudioDecoderConfig new_config(
      normal.codec(), normal.sample_format(),
      ChannelLayoutConfig::Guess(normal.channels()),
      normal.samples_per_second() * 2, std::vector<uint8_t>(),
      EncryptionScheme::kUnencrypted);

  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(0)));
  stream_.ChangeConfig(new_config);
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(10)));

  // Flushing drops packets from both segments, but keeps the pending config so
  // the next read still signals `kConfigChanged` before post-flush packets.
  stream_.FlushBuffers();
  stream_.EnqueueBuffer(CreateBuffer(base::Milliseconds(20)));

  ReadFuture change;
  stream_.Read(2, change.GetCallback());
  EXPECT_EQ(change.Get<DemuxerStream::Status>(), DemuxerStream::kConfigChanged);
  EXPECT_TRUE(stream_.audio_decoder_config().Matches(new_config));

  ReadFuture read;
  stream_.Read(2, read.GetCallback());
  auto [status, buffers] = read.Take();
  EXPECT_EQ(status, DemuxerStream::kOk);
  ASSERT_EQ(buffers.size(), 1u);
  EXPECT_EQ(buffers[0]->timestamp(), base::Milliseconds(20));
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

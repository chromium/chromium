// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/base/media_track.h"
#include "media/base/media_util.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/filters/manifest_demuxer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SaveArg;

// Define a mock implementation of ManifestDemuxer::Engine for testing.
class MockEngine : public ManifestDemuxer::Engine {
 public:
  MOCK_METHOD(void,
              Initialize,
              (ManifestDemuxerEngineHost * demuxer,
               PipelineStatusCallback status_cb),
              (override));
  MOCK_METHOD(std::string, GetName, (), (const, override));
  MOCK_METHOD(void,
              OnTimeUpdate,
              (base::TimeDelta time,
               double playback_rate,
               ManifestDemuxer::DelayCallback cb),
              (override));
  MOCK_METHOD(bool, Seek, (base::TimeDelta time), (override));
  MOCK_METHOD(void, StartWaitingForSeek, (), (override));
  MOCK_METHOD(void, AbortPendingReads, (), (override));
  MOCK_METHOD(bool, IsSeekable, (), (override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (const, override));
  MOCK_METHOD(void, Stop, (), (override));
};

// Fixture for ManifestDemuxer tests.
class ManifestDemuxerTest : public ::testing::Test {
 public:
  ManifestDemuxerTest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        mock_host_(std::make_unique<NiceMock<MockDemuxerHost>>()) {
    auto mock_engine = std::make_unique<MockEngine>();
    mock_engine_ = mock_engine.get();
    manifest_demuxer_ = std::make_unique<ManifestDemuxer>(
        task_environment_.GetMainThreadTaskRunner(), std::move(mock_engine),
        media_log_.get());
  }

  ~ManifestDemuxerTest() override {
    manifest_demuxer_->GetChunkDemuxerForTesting()->MarkEndOfStream(
        PIPELINE_OK);
    manifest_demuxer_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD(void, MockInitComplete, (PipelineStatus status), ());
  MOCK_METHOD(void, MockSeekComplete, (PipelineStatus status), ());

  void CreateIdAndAppendInitSegment(const std::string& id) {
    auto* demuxer = manifest_demuxer_->GetChunkDemuxerForTesting();
    DCHECK_EQ(demuxer->AddId(id, "video/webm", "vorbis,vp8"),
              ChunkDemuxer::Status::kOk);

    demuxer->SetTracksWatcher(
        id, base::BindRepeating([](std::unique_ptr<MediaTracks>) {}));
    demuxer->SetParseWarningCallback(
        id, base::BindRepeating([](SourceBufferParseWarning) {}));

    scoped_refptr<DecoderBuffer> bear1 = ReadTestDataFile("bear-320x240.webm");
    DCHECK(demuxer->AppendToParseBuffer(id, bear1->data(), bear1->data_size()));
    for (;;) {
      base::TimeDelta start = base::Seconds(0), end = base::Seconds(10), offset;
      auto result = demuxer->RunSegmentParserLoop(id, start, end, &offset);
      if (result != StreamParser::ParseStatus::kSuccessHasMoreData) {
        DCHECK_EQ(result, StreamParser::ParseStatus::kSuccess);
        return;
      }
    }
  }

  void InitializeDemuxer() {
    // Chunk demuxer won't finish initialization until content starts being
    // added, and we don't have any mock content at this point.
    EXPECT_CALL(*this, MockInitComplete(_)).Times(1);

    // Mark the engine as initialized successfully.
    EXPECT_CALL(*mock_engine_, Initialize(_, _))
        .WillOnce(RunOnceCallback<1>(media::PIPELINE_OK));

    manifest_demuxer_->Initialize(
        mock_host_.get(), base::BindOnce(&ManifestDemuxerTest::MockInitComplete,
                                         base::Unretained(this)));
    CreateIdAndAppendInitSegment("test");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MediaLog> media_log_;
  std::unique_ptr<MockDemuxerHost> mock_host_;
  raw_ptr<MockEngine> mock_engine_;
  std::unique_ptr<ManifestDemuxer> manifest_demuxer_;
};

TEST_F(ManifestDemuxerTest, InitializeStartsTimeUpdate) {
  // When the engine is initialized and the chunk demuxer is opened
  // (not initialized), the |OnTimeUpdate| events start coming in.
  // posting `kNoTimestamp` back to this callback signals that we won't delay
  // and get another event.
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _))
      .WillOnce(RunOnceCallback<2>(kNoTimestamp));
  InitializeDemuxer();

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manifest_demuxer_->has_next_task_for_testing());
}

TEST_F(ManifestDemuxerTest, PlaybackRateChangeUpTriggersTimeUpdate) {
  ManifestDemuxer::DelayCallback delay_cb;
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _))
      .WillRepeatedly([&delay_cb](base::TimeDelta, double,
                                  ManifestDemuxer::DelayCallback cb) {
        delay_cb = std::move(cb);
      });

  // Initializing the demuxer will cause a time update event at time = 0.
  InitializeDemuxer();
  ASSERT_TRUE(!!delay_cb);

  // Setting the playback rate up while there is a pending event should do
  // nothing.
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _)).Times(0);
  manifest_demuxer_->SetPlaybackRate(0.2);

  // Respond to the loop, but request no new events.
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();

  // Setting the playback rate up again while there is not pending event
  // should trigger a new event.
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _))
      .WillRepeatedly([&delay_cb](base::TimeDelta, double,
                                  ManifestDemuxer::DelayCallback cb) {
        delay_cb = std::move(cb);
      });
  manifest_demuxer_->SetPlaybackRate(0.4);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!!delay_cb);

  // Respond to the loop, but request no new events.
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();

  // Setting the playback rate down should not trigger a new event, even
  // while there is no event pending.
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _)).Times(0);
  manifest_demuxer_->SetPlaybackRate(0.2);

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manifest_demuxer_->has_next_task_for_testing());
}

TEST_F(ManifestDemuxerTest, OnTimeUpdateUninterruptedBySeek) {
  ManifestDemuxer::DelayCallback delay_cb;
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _))
      .WillRepeatedly([&delay_cb](base::TimeDelta, double,
                                  ManifestDemuxer::DelayCallback cb) {
        delay_cb = std::move(cb);
      });
  InitializeDemuxer();
  ASSERT_TRUE(!!delay_cb);

  // a pending event is set, which won't be cleared until `delay_cb` is
  // executed.
  ASSERT_FALSE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  // Seek won't be called until we post delay_cb.
  EXPECT_CALL(*mock_engine_, Seek(_)).Times(0);
  EXPECT_CALL(*mock_engine_, StartWaitingForSeek()).Times(1);
  EXPECT_CALL(*this, MockSeekComplete(_)).Times(0);
  manifest_demuxer_->StartWaitingForSeek(base::Seconds(1));
  manifest_demuxer_->Seek(base::Seconds(1),
                          base::BindOnce(&ManifestDemuxerTest::MockSeekComplete,
                                         base::Unretained(this)));

  // we not have a pending seek and a pending event.
  ASSERT_TRUE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  // Return from the pending event. The pending seek will start, which will
  // kick off an async call to the chunk demuxer. We can make the engine
  // also request a new event to be called, which means that delay_cb will be
  // set again.
  EXPECT_CALL(*mock_engine_, Seek(_)).WillOnce(Return(true));
  std::move(delay_cb).Run(base::Seconds(10));
  task_environment_.RunUntilIdle();

  // There is still a pending seek, and we now have a new event.
  ASSERT_TRUE(!!delay_cb);
  ASSERT_TRUE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  // Executing this delay CB will trigger the seek to finish, since chunk
  // demuxer has already finished it's seek. After the seek is finished, it
  // will kick off another event, making the pending event check true and
  // causing a new `delay_cb` to be set.
  EXPECT_CALL(*this, MockSeekComplete(_)).Times(1);
  std::move(delay_cb).Run(base::Seconds(10));
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  // Running this event with no timestamp will cause the event loop to no longer
  // run. Only kicking off a seek or playback rate change will re-trigger it.
  ASSERT_TRUE(!!delay_cb);
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_FALSE(manifest_demuxer_->has_pending_event_for_testing());

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(manifest_demuxer_->has_next_task_for_testing());
}

TEST_F(ManifestDemuxerTest, CancelSeekAfterDemuxerBeforeEngine) {
  // What happens if we seek, the demuxer replies, and while waiting for the
  // engine to reply, we get a notice to cancel pending seek?

  ManifestDemuxer::DelayCallback delay_cb;
  EXPECT_CALL(*mock_engine_, OnTimeUpdate(_, _, _))
      .WillRepeatedly([&delay_cb](base::TimeDelta, double,
                                  ManifestDemuxer::DelayCallback cb) {
        delay_cb = std::move(cb);
      });

  // a pending event is set, which won't be cleared until `delay_cb` is
  // executed.
  InitializeDemuxer();
  ASSERT_TRUE(!!delay_cb);
  ASSERT_FALSE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  // Seek won't be called until we post delay_cb.
  EXPECT_CALL(*mock_engine_, StartWaitingForSeek());
  EXPECT_CALL(*mock_engine_, Seek(_)).Times(0);
  EXPECT_CALL(*this, MockSeekComplete(_)).Times(0);
  manifest_demuxer_->StartWaitingForSeek(base::Seconds(100));
  manifest_demuxer_->Seek(base::Seconds(100),
                          base::BindOnce(&ManifestDemuxerTest::MockSeekComplete,
                                         base::Unretained(this)));
  task_environment_.RunUntilIdle();

  // When we execute `delay_cb`, it will trigger `SeekInternal`, which will kick
  // off a call to ChunkDemuxer::Seek and will also recapture a new `delay_cb`.
  // The new `delay_cb` is bound to a task which completes the enigne seek step.
  // The chunk demuxer should have already responded, and the pending seek
  // should only be waiting on the engine.
  EXPECT_CALL(*mock_engine_, Seek(_)).WillOnce(Return(true));
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!!delay_cb);
  ASSERT_TRUE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_TRUE(manifest_demuxer_->has_pending_event_for_testing());

  EXPECT_CALL(*mock_engine_, AbortPendingReads());
  manifest_demuxer_->CancelPendingSeek(base::Seconds(5));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(manifest_demuxer_->get_media_time_for_testing(),
            base::Seconds(100));

  // Running `delay_cb` will finish the seek, and start a new update, even if
  // it runs with kNoTimestamp.
  EXPECT_CALL(*this, MockSeekComplete(_));
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!!delay_cb);
  ASSERT_EQ(manifest_demuxer_->get_media_time_for_testing(),
            base::Seconds(100));

  // Run it again to end the loop.
  std::move(delay_cb).Run(kNoTimestamp);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(!delay_cb);
  ASSERT_FALSE(manifest_demuxer_->has_pending_seek_for_testing());
  ASSERT_FALSE(manifest_demuxer_->has_pending_event_for_testing());
}

}  // namespace media

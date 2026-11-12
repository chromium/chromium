// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/file_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

namespace {

constexpr base::TimeDelta kSeekTarget = base::Milliseconds(100);
constexpr base::TimeDelta kSeekTolerance = base::Milliseconds(50);

// A `FileDataSource` that does not know its size, like an HTTP response
// without a Content-Length header.
class UnknownSizeFileDataSource : public FileDataSource {
 public:
  bool GetSize(int64_t* size_out) override { return false; }
};

// A `FileDataSource` that can hold reads until `Abort()` or `Stop()`, like a
// slow network source. This makes races between in-flight blocking work and
// teardown deterministic.
class BlockingFileDataSource : public FileDataSource {
 public:
  // Holds every read issued after this call. Runs `on_read_held` when the first
  // read is held.
  void BlockReads(base::OnceClosure on_read_held) {
    base::AutoLock lock(lock_);
    block_reads_ = true;
    on_read_held_ = std::move(on_read_held);
  }

  // Fails every read issued after this call with `DataSource::kReadError`.
  void FailReads() {
    base::AutoLock lock(lock_);
    fail_reads_ = true;
  }

  // FileDataSource implementation.
  void Read(int64_t position,
            base::span<uint8_t> data,
            DataSource::ReadCB read_cb) override {
    DataSource::ReadCB fail_cb;
    DataSource::ReadCB passthrough_cb;
    base::OnceClosure on_read_held;
    {
      base::AutoLock lock(lock_);
      if (fail_reads_) {
        fail_cb = std::move(read_cb);
      } else if (block_reads_) {
        CHECK(!held_read_cb_);
        held_read_cb_ = std::move(read_cb);
        on_read_held = std::move(on_read_held_);
      } else {
        passthrough_cb = std::move(read_cb);
      }
    }
    if (fail_cb) {
      std::move(fail_cb).Run(DataSource::kReadError);
      return;
    }
    if (passthrough_cb) {
      FileDataSource::Read(position, data, std::move(passthrough_cb));
      return;
    }
    if (on_read_held) {
      std::move(on_read_held).Run();
    }
  }

  void Abort() override {
    ReleaseHeldRead();
    FileDataSource::Abort();
  }

  void Stop() override {
    ReleaseHeldRead();
    FileDataSource::Stop();
  }

 private:
  // Stops holding reads, and completes the held read, if any, as aborted.
  void ReleaseHeldRead() {
    DataSource::ReadCB read_cb;
    {
      base::AutoLock lock(lock_);
      block_reads_ = false;
      read_cb = std::move(held_read_cb_);
    }
    if (read_cb) {
      std::move(read_cb).Run(DataSource::kAborted);
    }
  }

  base::Lock lock_;
  bool block_reads_ GUARDED_BY(lock_) = false;
  bool fail_reads_ GUARDED_BY(lock_) = false;
  base::OnceClosure on_read_held_ GUARDED_BY(lock_);
  DataSource::ReadCB held_read_cb_ GUARDED_BY(lock_);
};

}  // namespace

class SymphoniaDemuxerTest : public testing::Test {
 public:
  SymphoniaDemuxerTest() = default;
  ~SymphoniaDemuxerTest() override {
    if (demuxer_) {
      demuxer_->Stop();
    }
  }

  void CreateDemuxer(const std::string& filename) {
    CreateDemuxerForFile(GetTestDataFilePath(filename));
  }

  // Creates a demuxer reading a temporary file containing `contents`.
  void CreateDemuxerWithContents(std::string_view contents) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    const base::FilePath path = temp_dir_.GetPath().AppendASCII("media");
    CHECK(base::WriteFile(path, contents));
    CreateDemuxerForFile(path);
  }

  void CreateDemuxerForFile(const base::FilePath& path) {
    auto data_source = std::make_unique<FileDataSource>();
    CHECK(data_source->Initialize(path));
    CreateDemuxerForDataSource(std::move(data_source));
  }

  void CreateDemuxerForDataSource(std::unique_ptr<FileDataSource> data_source) {
    data_source_ = std::move(data_source);
    demuxer_ = std::make_unique<SymphoniaDemuxer>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), data_source_.get(),
        base::DoNothing(),
        base::BindRepeating(&SymphoniaDemuxerTest::OnMediaTracksUpdated,
                            base::Unretained(this)),
        media_log_.Clone());
  }

  void InitializeDemuxer(PipelineStatus expected_status = PIPELINE_OK) {
    EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
    base::test::TestFuture<PipelineStatus> status;
    demuxer_->Initialize(&host_, status.GetCallback());
    EXPECT_EQ(status.Get(), expected_status);
  }

  SymphoniaDemuxerStream* GetStream(DemuxerStream::Type type) {
    for (DemuxerStream* stream : demuxer_->GetAllStreams()) {
      if (stream->type() == type) {
        return static_cast<SymphoniaDemuxerStream*>(stream);
      }
    }
    return nullptr;
  }

  // Reads a single buffer from `stream`. Returns null if the read did not
  // succeed.
  scoped_refptr<DecoderBuffer> ReadBuffer(DemuxerStream* stream) {
    ReadFuture future;
    stream->Read(1, future.GetCallback());
    auto [status, buffers] = future.Take();
    if (status != DemuxerStream::kOk || buffers.empty()) {
      return nullptr;
    }
    return std::move(buffers[0]);
  }

  // Reads every packet from the audio stream, checking that timestamps are
  // monotonic and that the stream ends with an end-of-stream buffer.
  void ExpectReadsAllPackets() {
    SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
    ASSERT_NE(stream, nullptr);

    int packet_count = 0;
    base::TimeDelta last_pts = base::TimeDelta::Min();
    while (true) {
      scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
      ASSERT_NE(buffer, nullptr);
      if (buffer->end_of_stream()) {
        break;
      }
      EXPECT_GT(buffer->size(), 0u);
      EXPECT_GE(buffer->timestamp(), last_pts);
      last_pts = buffer->timestamp();
      ++packet_count;
    }
    EXPECT_GT(packet_count, 0);
  }

  void Seek(base::TimeDelta time,
            PipelineStatus expected_status = PIPELINE_OK) {
    base::test::TestFuture<PipelineStatus> status;
    demuxer_->Seek(time, status.GetCallback());
    EXPECT_EQ(status.Get(), expected_status);
  }

  void OnMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks) {
    media_tracks_ = std::move(tracks);
    if (on_media_tracks_updated_) {
      std::move(on_media_tracks_updated_).Run();
    }
  }

 protected:
  using ReadFuture = base::test::TestFuture<DemuxerStream::Status,
                                            DemuxerStream::DecoderBufferVector>;

  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  StrictMock<MockDemuxerHost> host_;
  NiceMock<MockMediaLog> media_log_;
  std::unique_ptr<FileDataSource> data_source_;
  std::unique_ptr<SymphoniaDemuxer> demuxer_;
  std::unique_ptr<MediaTracks> media_tracks_;
  // Run when the demuxer publishes its tracks, which happens on the media
  // sequence after probing and before the first packet read is posted.
  base::OnceClosure on_media_tracks_updated_;
};

TEST_F(SymphoniaDemuxerTest, InitializeWav) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->type(), DemuxerStream::AUDIO);

  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.codec(), AudioCodec::kPCM);
  EXPECT_EQ(config.sample_format(), kSampleFormatS16);
  EXPECT_EQ(config.samples_per_second(), 44100);
  EXPECT_EQ(config.channels(), 1);
  EXPECT_GT(stream->duration(), base::TimeDelta());

  ASSERT_NE(media_tracks_, nullptr);
  EXPECT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerWAV);
  EXPECT_GE(demuxer_->GetMemoryUsage(), 0);
}

TEST_F(SymphoniaDemuxerTest, ReadsAllPacketsWav) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();
  ExpectReadsAllPackets();
}

TEST_F(SymphoniaDemuxerTest, ReadsAllPacketsFlac) {
  CreateDemuxer("sfx.flac");
  InitializeDemuxer();
  ExpectReadsAllPackets();
}

// MP3 has no length in its headers, so this reads through to end of file.
TEST_F(SymphoniaDemuxerTest, ReadsAllPacketsMp3) {
  CreateDemuxer("sfx.mp3");
  InitializeDemuxer();
  ExpectReadsAllPackets();
}

TEST_F(SymphoniaDemuxerTest, SeeksAccuratelyWav) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  Seek(kSeekTarget);

  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_LE((buffer->timestamp() - kSeekTarget).magnitude(), kSeekTolerance);
}

TEST_F(SymphoniaDemuxerTest, InitializeFlac) {
  CreateDemuxer("sfx.flac");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.codec(), AudioCodec::kFLAC);
  EXPECT_GT(stream->duration(), base::TimeDelta());

  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
  EXPECT_GT(buffer->size(), 0u);

  ASSERT_NE(media_tracks_, nullptr);
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerFLAC);
}

TEST_F(SymphoniaDemuxerTest, InitializeMp3) {
  CreateDemuxer("sfx.mp3");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.codec(), AudioCodec::kMP3);

  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());

  ASSERT_NE(media_tracks_, nullptr);
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerMP3);
}

TEST_F(SymphoniaDemuxerTest, InitializeFailsWithTruncatedHeader) {
  CreateDemuxerWithContents("RIFF");
  InitializeDemuxer(DEMUXER_ERROR_COULD_NOT_OPEN);
}

TEST_F(SymphoniaDemuxerTest, InitializeFailsWithInvalidData) {
  CreateDemuxerWithContents("GARBAGE NOT MEDIA DATA");
  InitializeDemuxer(DEMUXER_ERROR_COULD_NOT_OPEN);
}

TEST_F(SymphoniaDemuxerTest, StopAbortsPendingRead) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  // Flush queued buffers so the stream queue is empty without being at EOS.
  stream->FlushBuffers();

  ReadFuture future;
  stream->Read(1, future.GetCallback());
  demuxer_->Stop();
  EXPECT_EQ(future.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);
}

// Like `FFmpegDemuxer`, a read error in the middle of the stream ends the
// stream instead of failing playback, so a truncated file plays up to the point
// where its data stops.
// TODO(crbug.com/584374580): Also cover truncated FLAC, which Symphonia
// currently reports as an empty stream.
TEST_F(SymphoniaDemuxerTest, TruncatedFileEndsStreamAtReadError) {
  std::string contents;
  ASSERT_TRUE(
      base::ReadFileToString(GetTestDataFilePath("sfx_s16le.wav"), &contents));
  // Cut the file in the middle of the data chunk, so Symphonia hits an
  // unexpected end of file partway through.
  CreateDemuxerWithContents(
      std::string_view(contents).substr(0, contents.size() * 2 / 3));

  EXPECT_CALL(media_log_, DoAddLogRecordLogString(_)).Times(AnyNumber());
  EXPECT_MEDIA_LOG(HasSubstr("read_packet failed"));
  InitializeDemuxer();

  // `host_` is a `StrictMock`, so this also checks that no demuxer error is
  // reported.
  ExpectReadsAllPackets();
}

// A `DataSource::kReadError` during active packet demuxing must report
// `PIPELINE_ERROR_READ` to the `DemuxerHost` and end the stream.
TEST_F(SymphoniaDemuxerTest, MidStreamDataSourceReadErrorReportsDemuxerError) {
  // A large file, so that packet reads still need `DataSource` reads after
  // probing.
  auto data_source = std::make_unique<BlockingFileDataSource>();
  ASSERT_TRUE(
      data_source->Initialize(GetTestDataFilePath("silent-section.mp3")));
  BlockingFileDataSource* blocking_data_source = data_source.get();
  CreateDemuxerForDataSource(std::move(data_source));

  base::test::TestFuture<PipelineStatus> demuxer_error;
  EXPECT_CALL(host_, OnDemuxerError(_)).WillOnce([&](PipelineStatus status) {
    demuxer_error.SetValue(status);
  });

  // Fail `DataSource` reads once tracks are published, right before the
  // demuxer starts reading packets.
  on_media_tracks_updated_ =
      base::BindLambdaForTesting([&] { blocking_data_source->FailReads(); });
  InitializeDemuxer();

  EXPECT_EQ(demuxer_error.Get(), PIPELINE_ERROR_READ);

  // Drain any packets already buffered before the failed `DataSource::Read()`,
  // and verify the stream ends well short of the ~14.4s full file duration.
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  base::TimeDelta last_pts;
  bool saw_eos = false;
  while (true) {
    scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
    ASSERT_NE(buffer, nullptr);
    if (buffer->end_of_stream()) {
      saw_eos = true;
      break;
    }
    last_pts = buffer->timestamp();
  }
  EXPECT_TRUE(saw_eos);
  EXPECT_LT(last_pts, base::Seconds(5));
}

// `Stop()` while the container is still being probed must abort
// initialization and unblock the probe, and the stale init reply must be
// dropped.
TEST_F(SymphoniaDemuxerTest, StopDuringInitializeAbortsInitialization) {
  auto data_source = std::make_unique<BlockingFileDataSource>();
  ASSERT_TRUE(data_source->Initialize(GetTestDataFilePath("sfx_s16le.wav")));
  base::test::TestFuture<void> read_held;
  data_source->BlockReads(read_held.GetSequenceBoundCallback());
  CreateDemuxerForDataSource(std::move(data_source));

  // `host_` is a `StrictMock`, and probing never completes, so `SetDuration()`
  // must not be called.
  base::test::TestFuture<PipelineStatus> init_status;
  demuxer_->Initialize(&host_, init_status.GetCallback());
  ASSERT_TRUE(read_held.Wait());
  EXPECT_FALSE(init_status.IsReady());

  demuxer_->Stop();
  EXPECT_TRUE(init_status.IsReady());
  EXPECT_EQ(init_status.Get(), PIPELINE_ERROR_ABORT);

  // Wait for the unblocked probe to finish on the blocking sequence; this hangs
  // if `Stop()` did not unblock it. Then run its reply, which is queued on this
  // sequence ahead of `drained` and must be dropped.
  base::ThreadPoolInstance::Get()->FlushForTesting();
  base::test::TestFuture<void> drained;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, drained.GetCallback());
  EXPECT_TRUE(drained.Wait());
}

// `AbortPendingReads()` while a seek is queued on the blocking sequence must
// complete the seek, drop its stale reply, and leave the demuxer able to seek
// and read again.
TEST_F(SymphoniaDemuxerTest, AbortPendingReadsDuringSeekCompletesSeek) {
  // A large file, so that packet reads still need `DataSource` reads after
  // probing.
  auto data_source = std::make_unique<BlockingFileDataSource>();
  ASSERT_TRUE(
      data_source->Initialize(GetTestDataFilePath("silent-section.mp3")));
  BlockingFileDataSource* blocking_data_source = data_source.get();
  CreateDemuxerForDataSource(std::move(data_source));

  // Start holding reads when the tracks are published, just before the demuxer
  // starts reading packets. The seek below then queues behind a held read.
  base::test::TestFuture<void> read_held;
  on_media_tracks_updated_ = base::BindLambdaForTesting([&] {
    blocking_data_source->BlockReads(read_held.GetSequenceBoundCallback());
  });
  InitializeDemuxer();
  ASSERT_TRUE(read_held.Wait());

  base::test::TestFuture<PipelineStatus> seek_status;
  demuxer_->Seek(kSeekTarget, seek_status.GetCallback());
  EXPECT_FALSE(seek_status.IsReady());

  demuxer_->AbortPendingReads();
  EXPECT_TRUE(seek_status.IsReady());
  EXPECT_EQ(seek_status.Get(), PIPELINE_OK);

  // A new seek and read must still work. The new seek runs on the blocking
  // sequence after the aborted read and the stale seek, so their replies, which
  // must be dropped, arrive before its reply.
  Seek(kSeekTarget);
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST_F(SymphoniaDemuxerTest, OnTracksChangedDisablesAndReenables) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  ASSERT_NE(media_tracks_, nullptr);
  ASSERT_EQ(media_tracks_->tracks().size(), 1u);
  MediaTrack::Id track_id = media_tracks_->tracks()[0]->track_id();

  // Disabling the track reports no enabled stream.
  base::test::TestFuture<DemuxerStream*> disable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, std::nullopt,
                            base::TimeDelta(), disable_future.GetCallback());
  EXPECT_EQ(disable_future.Get(), nullptr);

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_FALSE(stream->is_enabled());

  // Re-enabling the track reports its stream.
  base::test::TestFuture<DemuxerStream*> enable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, track_id, base::TimeDelta(),
                            enable_future.GetCallback());
  EXPECT_EQ(enable_future.Get(), stream);
  EXPECT_TRUE(stream->is_enabled());
}

TEST_F(SymphoniaDemuxerTest, CancelPendingSeekAbortsReads) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  stream->FlushBuffers();

  ReadFuture future;
  stream->Read(1, future.GetCallback());
  demuxer_->CancelPendingSeek(kSeekTarget);
  EXPECT_EQ(future.Get<DemuxerStream::Status>(), DemuxerStream::kAborted);

  // Subsequent seeks and reads must still succeed after canceling a seek.
  Seek(kSeekTarget);
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST_F(SymphoniaDemuxerTest, SeekPastEndEndsStream) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  // Seeking past the last packet is not an error: the stream just ends.
  Seek(base::Seconds(100));
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_TRUE(buffer->end_of_stream());

  // Seeking back into the media reads packets again.
  Seek(kSeekTarget);
  buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
}

TEST_F(SymphoniaDemuxerTest, OnlyFirstAudioTrackStartsEnabled) {
  CreateDemuxer("multitrack-3video-2audio.webm");
  InitializeDemuxer();

  std::vector<raw_ptr<DemuxerStream>> streams = demuxer_->GetAllStreams();
  ASSERT_EQ(streams.size(), 2u);
  auto* enabled_stream = static_cast<SymphoniaDemuxerStream*>(streams[0].get());
  auto* disabled_stream =
      static_cast<SymphoniaDemuxerStream*>(streams[1].get());
  EXPECT_TRUE(enabled_stream->is_enabled());
  EXPECT_FALSE(disabled_stream->is_enabled());

  ASSERT_NE(media_tracks_, nullptr);
  ASSERT_EQ(media_tracks_->tracks().size(), 2u);
  EXPECT_TRUE(media_tracks_->tracks()[0]->enabled());
  EXPECT_FALSE(media_tracks_->tracks()[1]->enabled());
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerWEBM);

  // Packets for the disabled track are dropped rather than buffered.
  ExpectReadsAllPackets();
  EXPECT_EQ(disabled_stream->GetMemoryUsage(), 0);
}

TEST_F(SymphoniaDemuxerTest, UnknownDurationIsSetAtEndOfStream) {
  // Without a file size, like an HTTP response with no Content-Length header,
  // Symphonia can't estimate the duration of an MP3 without a Xing header.
  auto data_source = std::make_unique<UnknownSizeFileDataSource>();
  ASSERT_TRUE(
      data_source->Initialize(GetTestDataFilePath("silent-section.mp3")));
  CreateDemuxerForDataSource(std::move(data_source));
  {
    testing::InSequence sequence;
    EXPECT_CALL(host_, SetDuration(kInfiniteDuration));
    // The file is about 14.4 seconds long.
    EXPECT_CALL(host_,
                SetDuration(testing::AllOf(testing::Gt(base::Seconds(14)),
                                           testing::Lt(base::Seconds(15)))));
  }
  base::test::TestFuture<PipelineStatus> status;
  demuxer_->Initialize(&host_, status.GetCallback());
  EXPECT_EQ(status.Get(), PIPELINE_OK);

  ExpectReadsAllPackets();
}

TEST_F(SymphoniaDemuxerTest, ConstructedOnMainThreadAndUsedOnMediaSequence) {
  data_source_ = std::make_unique<FileDataSource>();
  ASSERT_TRUE(data_source_->Initialize(GetTestDataFilePath("sfx_s16le.wav")));

  // Like `DemuxerManager`, construct and destroy the demuxer on the main
  // thread, but drive it on a separate media sequence.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  demuxer_ = std::make_unique<SymphoniaDemuxer>(
      media_task_runner, data_source_.get(), base::DoNothing(),
      base::DoNothing(), media_log_.Clone());

  EXPECT_CALL(host_, SetDuration(_));
  base::test::TestFuture<PipelineStatus> init_status;
  media_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&SymphoniaDemuxer::Initialize,
                                base::Unretained(demuxer_.get()), &host_,
                                init_status.GetSequenceBoundCallback()));
  EXPECT_EQ(init_status.Get(), PIPELINE_OK);

  base::test::TestFuture<void> stopped;
  media_task_runner->PostTaskAndReply(
      FROM_HERE, base::BindLambdaForTesting([&] { demuxer_->Stop(); }),
      stopped.GetCallback());
  ASSERT_TRUE(stopped.Wait());

  demuxer_.reset();
}

}  // namespace media

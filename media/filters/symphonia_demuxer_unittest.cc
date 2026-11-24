// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_demuxer.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/container_names.h"
#include "media/base/data_source.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/media_tracks.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_media_log.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/demuxer_manager.h"
#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/ffmpeg_demuxer.h"
#endif  // BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/file_data_source.h"
#include "media/filters/mock_demuxer_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::HasSubstr;
using ::testing::NiceMock;
using ::testing::Property;
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

// A `FileDataSource` that records the bitrate the demuxer reports.
class BitrateRecordingFileDataSource : public FileDataSource {
 public:
  void SetBitrate(int bitrate) override { bitrate_ = bitrate; }
  int bitrate() const { return bitrate_; }

 private:
  int bitrate_ = 0;
};

// A `FileDataSource` that can hold reads until `Abort()` or `Stop()`, like a
// slow network source. This makes races between in-flight blocking work and
// teardown deterministic. It also records whether it was stopped, since
// `FileDataSource::Stop()` itself does nothing.
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
    {
      base::AutoLock lock(lock_);
      stopped_ = true;
    }
    ReleaseHeldRead();
    FileDataSource::Stop();
  }

  bool stopped() {
    base::AutoLock lock(lock_);
    return stopped_;
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
  bool stopped_ GUARDED_BY(lock_) = false;
};

// The per-container `kSymphonia*Demuxing` features.
std::vector<base::test::FeatureRef> SymphoniaContainerFeatures() {
  return {kSymphoniaAacDemuxing, kSymphoniaFlacDemuxing, kSymphoniaIsomDemuxing,
          kSymphoniaMkvDemuxing, kSymphoniaMp3Demuxing,  kSymphoniaOggDemuxing,
          kSymphoniaRiffDemuxing};
}

// Sets feature state. Used as a base class so the state is set before a
// fixture's `TaskEnvironment` starts threads that may read features.
class ScopedFeatureState {
 protected:
  ScopedFeatureState(const std::vector<base::test::FeatureRef>& enabled,
                     const std::vector<base::test::FeatureRef>& disabled) {
    feature_list_.InitWithFeatures(enabled, disabled);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

class SymphoniaDemuxerTest : public testing::Test, public ScopedFeatureState {
 public:
  SymphoniaDemuxerTest()
      : SymphoniaDemuxerTest(SymphoniaContainerFeatures(), {}) {}
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
    auto data_source = std::make_unique<BlockingFileDataSource>();
    CHECK(data_source->Initialize(path));
    file_data_source_ = data_source.get();
    CreateDemuxerForDataSource(std::move(data_source));
  }

  void CreateDemuxerForDataSource(std::unique_ptr<FileDataSource> data_source) {
    data_source_ = std::move(data_source);
    demuxer_ = std::make_unique<SymphoniaDemuxer>(
        base::SingleThreadTaskRunner::GetCurrentDefault(), data_source_.get(),
        base::DoNothing(),
        base::BindRepeating(&SymphoniaDemuxerTest::OnMediaTracksUpdated,
                            base::Unretained(this)),
        media_log_.Clone(), allow_fallback_);
  }

  void InitializeDemuxer(PipelineStatus expected_status = PIPELINE_OK) {
    EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
    EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());
    base::test::TestFuture<PipelineStatus> status;
    demuxer_->Initialize(&host_, status.GetCallback());
    EXPECT_EQ(status.Get(), expected_status) << status.Get().message();
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

  // Expects that the demuxer did not stop `data_source_`, so that another
  // demuxer can still read it. Only for data sources made by
  // `CreateDemuxerForFile()`.
  void ExpectDataSourceNotStopped() {
    EXPECT_FALSE(file_data_source_->stopped());
    std::array<uint8_t, 4> buffer;
    base::test::TestFuture<int> bytes_read;
    data_source_->Read(0, buffer, bytes_read.GetCallback());
    EXPECT_EQ(bytes_read.Get(), static_cast<int>(buffer.size()));
  }

 protected:
  using ReadFuture = base::test::TestFuture<DemuxerStream::Status,
                                            DemuxerStream::DecoderBufferVector>;

  SymphoniaDemuxerTest(const std::vector<base::test::FeatureRef>& enabled,
                       const std::vector<base::test::FeatureRef>& disabled)
      : ScopedFeatureState(enabled, disabled) {}

  // Expects that initialization recorded `outcome` to UMA, and `container`
  // as the fallback container if set.
  void ExpectInitOutcome(SymphoniaDemuxer::InitOutcome outcome,
                         std::optional<container_names::MediaContainerName>
                             container = std::nullopt) {
    histogram_tester_.ExpectUniqueSample(
        "Media.Audio.Symphonia.DemuxerInitResult", outcome, 1);
    if (container) {
      histogram_tester_.ExpectUniqueSample(
          "Media.Audio.Symphonia.DemuxerFallbackContainer", *container, 1);
    } else {
      histogram_tester_.ExpectTotalCount(
          "Media.Audio.Symphonia.DemuxerFallbackContainer", 0);
    }
  }

  base::test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  base::ScopedTempDir temp_dir_;
  StrictMock<MockDemuxerHost> host_;
  NiceMock<MockMediaLog> media_log_;
  std::unique_ptr<FileDataSource> data_source_;
  // `data_source_`, if made by `CreateDemuxerForFile()`.
  raw_ptr<BlockingFileDataSource> file_data_source_ = nullptr;
  std::unique_ptr<SymphoniaDemuxer> demuxer_;
  std::unique_ptr<MediaTracks> media_tracks_;
  // Passed to the demuxer created by `CreateDemuxer*()`.
  bool allow_fallback_ = true;
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
  EXPECT_EQ(demuxer_->GetDemuxerType(), DemuxerType::kSymphoniaDemuxer);
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerWAV);
  EXPECT_GE(demuxer_->GetMemoryUsage(), 0);
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kSuccess);
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

TEST_F(SymphoniaDemuxerTest, FlacSampleFormatFollowsBitDepth) {
  struct {
    const char* filename;
    SampleFormat expected;
  } constexpr kCases[] = {
      {"bear.flac", kSampleFormatS16},
      {"sfx.flac", kSampleFormatS32},
      {"sine-s32-stereo.flac", kSampleFormatS32},
  };
  for (const auto& test_case : kCases) {
    SCOPED_TRACE(test_case.filename);
    CreateDemuxer(test_case.filename);
    InitializeDemuxer();
    SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->audio_decoder_config().sample_format(),
              test_case.expected);
    demuxer_->Stop();
    demuxer_.reset();
    testing::Mock::VerifyAndClearExpectations(&host_);
  }
}

#if BUILDFLAG(ENABLE_FFMPEG)
// SymphoniaDemuxer must report the same FLAC sample format as FFmpegDemuxer.
TEST_F(SymphoniaDemuxerTest, FlacSampleFormatMatchesFFmpegDemuxer) {
  for (const char* filename :
       {"bear.flac", "sfx.flac", "sine-s32-stereo.flac"}) {
    SCOPED_TRACE(filename);
    FileDataSource data_source;
    ASSERT_TRUE(data_source.Initialize(GetTestDataFilePath(filename)));
    FFmpegDemuxer ffmpeg_demuxer(
        base::SingleThreadTaskRunner::GetCurrentDefault(), &data_source,
        base::DoNothing(), base::DoNothing(), &media_log_,
        /*is_local_file=*/true);
    EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
    base::test::TestFuture<PipelineStatus> status;
    ffmpeg_demuxer.Initialize(&host_, status.GetCallback());
    ASSERT_EQ(status.Get(), PIPELINE_OK);
    DemuxerStream* ffmpeg_stream = nullptr;
    for (DemuxerStream* stream : ffmpeg_demuxer.GetAllStreams()) {
      if (stream->type() == DemuxerStream::AUDIO) {
        ffmpeg_stream = stream;
      }
    }
    ASSERT_NE(ffmpeg_stream, nullptr);
    ffmpeg_demuxer.Stop();

    CreateDemuxer(filename);
    InitializeDemuxer();
    SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->audio_decoder_config().sample_format(),
              ffmpeg_stream->audio_decoder_config().sample_format());
    demuxer_->Stop();
    demuxer_.reset();
    testing::Mock::VerifyAndClearExpectations(&host_);
  }
}
#endif  // BUILDFLAG(ENABLE_FFMPEG)

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

TEST_F(SymphoniaDemuxerTest, ReadsAllPacketsChainedOgg) {
  // double-sfx.ogg is two chained copies of sfx.ogg. The second stream's
  // timestamps restart from zero inside the file, but must keep increasing,
  // and `DemuxerHost::SetDuration()` must grow past the first link's ~350 ms
  // probed length.
  CreateDemuxer("double-sfx.ogg");
  base::TimeDelta latest_duration;
  EXPECT_CALL(host_, SetDuration(_))
      .WillRepeatedly(testing::SaveArg<0>(&latest_duration));
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());
  base::test::TestFuture<PipelineStatus> init_status;
  demuxer_->Initialize(&host_, init_status.GetCallback());
  ASSERT_EQ(init_status.Get(), PIPELINE_OK);

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_TRUE(stream->SupportsConfigChanges());

  base::TimeDelta last_pts = base::TimeDelta::Min();
  while (true) {
    ReadFuture future;
    stream->Read(1, future.GetCallback());
    auto [status, buffers] = future.Take();
    if (status == DemuxerStream::kConfigChanged) {
      continue;
    }
    ASSERT_EQ(status, DemuxerStream::kOk);
    ASSERT_EQ(buffers.size(), 1u);
    if (buffers[0]->end_of_stream()) {
      break;
    }
    EXPECT_GE(buffers[0]->timestamp(), last_pts);
    last_pts = buffers[0]->timestamp();
  }

  // Each copy of sfx.ogg is about 350 ms long.
  constexpr base::TimeDelta kMinChainedDuration = base::Milliseconds(500);
  EXPECT_GT(last_pts, kMinChainedDuration);
  EXPECT_GT(latest_duration, kMinChainedDuration);
}

TEST_F(SymphoniaDemuxerTest, ChainedOggSwitchesAudioDecoderConfig) {
  std::string vorbis;
  std::string opus;
  ASSERT_TRUE(base::ReadFileToString(GetTestDataFilePath("sfx.ogg"), &vorbis));
  ASSERT_TRUE(
      base::ReadFileToString(GetTestDataFilePath("sfx-opus.ogg"), &opus));
  CreateDemuxerWithContents(vorbis + opus);
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->audio_decoder_config().codec(), AudioCodec::kVorbis);

  bool saw_config_change = false;
  int packets_after_config_change = 0;
  while (true) {
    ReadFuture future;
    stream->Read(1, future.GetCallback());
    auto [status, buffers] = future.Take();
    if (status == DemuxerStream::kConfigChanged) {
      saw_config_change = true;
      EXPECT_EQ(stream->audio_decoder_config().codec(), AudioCodec::kOpus);
      continue;
    }
    ASSERT_EQ(status, DemuxerStream::kOk);
    ASSERT_EQ(buffers.size(), 1u);
    if (buffers[0]->end_of_stream()) {
      break;
    }
    if (saw_config_change) {
      ++packets_after_config_change;
    }
  }
  EXPECT_TRUE(saw_config_change);
  EXPECT_GT(packets_after_config_change, 0);
}

TEST_F(SymphoniaDemuxerTest, ChainedOggRejectsMidStreamVideoTrack) {
  std::string audio;
  std::string video;
  ASSERT_TRUE(base::ReadFileToString(GetTestDataFilePath("sfx.ogg"), &audio));
  ASSERT_TRUE(base::ReadFileToString(GetTestDataFilePath("bear.ogv"), &video));
  CreateDemuxerWithContents(audio + video);

  // Hitting the multi-track audio+video link triggers `PIPELINE_ERROR_DECODE`
  // and stops further packet reads.
  EXPECT_CALL(host_, OnDemuxerError(_)).WillOnce([&](PipelineStatus status) {
    EXPECT_EQ(status, PIPELINE_ERROR_DECODE);
    demuxer_->AbortPendingReads();
  });
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  while (scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream)) {
    ASSERT_FALSE(buffer->end_of_stream());
  }
}

TEST_F(SymphoniaDemuxerTest, InitializeFailsWithTruncatedHeader) {
  // A WAVE file header with no chunks after it. Symphonia recognizes the
  // container but can't read it, which is an error rather than a fallback.
  CreateDemuxerWithContents(std::string_view("RIFF\x04\0\0\0WAVE", 12));
  InitializeDemuxer(DEMUXER_ERROR_COULD_NOT_OPEN);
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kError);
}

TEST_F(SymphoniaDemuxerTest, UnrecognizedContainerFallsBack) {
  CreateDemuxerWithContents("GARBAGE NOT MEDIA DATA");
  InitializeDemuxer(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  EXPECT_EQ(media_tracks_, nullptr);
  ExpectInitOutcome(
      SymphoniaDemuxer::InitOutcome::kFallbackSniffedContainerDisabled,
      container_names::MediaContainerName::kContainerUnknown);

  demuxer_->Stop();
  ExpectDataSourceNotStopped();
}

TEST_F(SymphoniaDemuxerTest, VideoTrackFallsBack) {
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  EXPECT_EQ(media_tracks_, nullptr);
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kFallbackVideoTrack,
                    container_names::MediaContainerName::kContainerWEBM);

  demuxer_->Stop();
  ExpectDataSourceNotStopped();
}

class SymphoniaDemuxerRiffDisabledTest : public SymphoniaDemuxerTest {
 public:
  SymphoniaDemuxerRiffDisabledTest()
      : SymphoniaDemuxerTest(/*enabled=*/{kSymphoniaFlacDemuxing},
                             /*disabled=*/{kSymphoniaRiffDemuxing}) {}
};

TEST_F(SymphoniaDemuxerRiffDisabledTest, DisabledContainerFallsBack) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  EXPECT_EQ(media_tracks_, nullptr);
  ExpectInitOutcome(
      SymphoniaDemuxer::InitOutcome::kFallbackSniffedContainerDisabled,
      container_names::MediaContainerName::kContainerWAV);

  demuxer_->Stop();
  ExpectDataSourceNotStopped();
}

TEST_F(SymphoniaDemuxerRiffDisabledTest, EnabledContainerStillPlays) {
  CreateDemuxer("sfx.flac");
  InitializeDemuxer();
  ExpectReadsAllPackets();
}

class SymphoniaDemuxerMp3OnlyTest : public SymphoniaDemuxerTest {
 public:
  SymphoniaDemuxerMp3OnlyTest()
      : SymphoniaDemuxerTest(/*enabled=*/{kSymphoniaMp3Demuxing},
                             /*disabled=*/{kSymphoniaFlacDemuxing}) {}
};

namespace {

// Returns an empty ID3v2.4 tag.
std::string EmptyId3v2Tag() {
  return std::string("ID3\x04\0\0\0\0\0\0", 10);
}

std::string ReadTestFile(std::string_view name) {
  std::string contents;
  CHECK(base::ReadFileToString(GetTestDataFilePath(name), &contents));
  return contents;
}

}  // namespace

TEST_F(SymphoniaDemuxerMp3OnlyTest, FallsBackForId3TaggedAdts) {
  // `DetermineContainer()` alone reports MP3 for anything after an ID3 tag.
  // The ADTS data after the tag decides the container.
  CreateDemuxerWithContents(EmptyId3v2Tag() + ReadTestFile("sfx.adts"));
  InitializeDemuxer(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  ExpectInitOutcome(
      SymphoniaDemuxer::InitOutcome::kFallbackSniffedContainerDisabled,
      container_names::MediaContainerName::kContainerAAC);
}

TEST_F(SymphoniaDemuxerMp3OnlyTest, FallsBackWhenProbedContainerIsDisabled) {
  // Two ID3 tags before FLAC data. The sniff skips only the first tag, so the
  // stream looks like MP3, which is enabled, but Symphonia finds FLAC, which
  // is not.
  const std::string flac = ReadTestFile("sfx.flac");
  ASSERT_EQ(container_names::DetermineContainer(
                base::as_byte_span(EmptyId3v2Tag() + flac)),
            container_names::MediaContainerName::kContainerMP3);

  CreateDemuxerWithContents(EmptyId3v2Tag() + EmptyId3v2Tag() + flac);
  InitializeDemuxer(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  EXPECT_EQ(media_tracks_, nullptr);
  ExpectInitOutcome(
      SymphoniaDemuxer::InitOutcome::kFallbackProbedContainerDisabled,
      container_names::MediaContainerName::kContainerFLAC);
}

class SymphoniaDemuxerFlacOnlyTest : public SymphoniaDemuxerTest {
 public:
  SymphoniaDemuxerFlacOnlyTest()
      : SymphoniaDemuxerTest(/*enabled=*/{kSymphoniaFlacDemuxing},
                             /*disabled=*/{kSymphoniaMp3Demuxing}) {}
};

TEST_F(SymphoniaDemuxerFlacOnlyTest, PlaysId3TaggedFlac) {
  CreateDemuxerWithContents(EmptyId3v2Tag() + ReadTestFile("sfx.flac"));
  InitializeDemuxer();
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kSuccess);
}

TEST_F(SymphoniaDemuxerRiffDisabledTest,
       PlaysDisabledContainerWithoutFallback) {
  allow_fallback_ = false;
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();
  ExpectReadsAllPackets();
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kSuccess);
}

TEST_F(SymphoniaDemuxerTest, PlaysAudioOfVideoFileWithoutFallback) {
  allow_fallback_ = false;
  CreateDemuxer("bear-320x240.webm");
  InitializeDemuxer();
  ASSERT_NE(media_tracks_, nullptr);
  ASSERT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->type(), MediaTrack::Type::kAudio);
}

TEST_F(SymphoniaDemuxerTest, UnrecognizedContainerFailsWithoutFallback) {
  allow_fallback_ = false;
  CreateDemuxerWithContents("GARBAGE NOT MEDIA DATA");
  InitializeDemuxer(DEMUXER_ERROR_COULD_NOT_OPEN);
  ExpectInitOutcome(SymphoniaDemuxer::InitOutcome::kError);

  // Without a fallback, stopping the demuxer stops the data source as usual.
  demuxer_->Stop();
  EXPECT_TRUE(file_data_source_->stopped());
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

TEST_F(SymphoniaDemuxerTest, ReenabledTrackResumesAtCurrentTime) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();
  ASSERT_NE(media_tracks_, nullptr);
  MediaTrack::Id track_id = media_tracks_->tracks()[0]->track_id();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  base::test::TestFuture<DemuxerStream*> disable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, std::nullopt,
                            base::TimeDelta(), disable_future.GetCallback());
  ASSERT_EQ(disable_future.Get(), nullptr);

  // Like `FFmpegDemuxer`, re-enabling a track seeks to the current playback
  // time, so the stream's first packet covers `kCurrentTime` rather than
  // continuing from wherever demuxing had reached.
  constexpr base::TimeDelta kCurrentTime = base::Milliseconds(200);
  base::test::TestFuture<DemuxerStream*> enable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, track_id, kCurrentTime,
                            enable_future.GetCallback());
  ASSERT_EQ(enable_future.Get(), stream);

  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  ASSERT_FALSE(buffer->end_of_stream());
  EXPECT_LE(buffer->timestamp(), kCurrentTime);
  EXPECT_GT(buffer->timestamp() + buffer->duration(), kCurrentTime);
}

TEST_F(SymphoniaDemuxerTest, StopCompletesPendingTrackChange) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();
  ASSERT_NE(media_tracks_, nullptr);
  MediaTrack::Id track_id = media_tracks_->tracks()[0]->track_id();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  base::test::TestFuture<DemuxerStream*> disable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, std::nullopt,
                            base::TimeDelta(), disable_future.GetCallback());
  ASSERT_EQ(disable_future.Get(), nullptr);

  // Stop before the track change seek replies; the change must still finish.
  base::test::TestFuture<DemuxerStream*> enable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, track_id,
                            base::Milliseconds(200),
                            enable_future.GetCallback());
  EXPECT_FALSE(enable_future.IsReady());
  demuxer_->Stop();
  EXPECT_TRUE(enable_future.IsReady());
  EXPECT_EQ(enable_future.Get(), stream);
}

TEST_F(SymphoniaDemuxerTest,
       SwitchingMultitrackAudioSeeksSecondTrackToCurrentTime) {
  CreateDemuxer("multitrack-2audio.webm");
  InitializeDemuxer();

  ASSERT_NE(media_tracks_, nullptr);
  ASSERT_EQ(media_tracks_->tracks().size(), 2u);
  std::vector<raw_ptr<DemuxerStream>> streams = demuxer_->GetAllStreams();
  ASSERT_EQ(streams.size(), 2u);
  auto* first_stream = static_cast<SymphoniaDemuxerStream*>(streams[0].get());
  auto* second_stream = static_cast<SymphoniaDemuxerStream*>(streams[1].get());

  // Advance demuxing on the first track past 1s before switching to the second
  // track at an earlier playback position.
  base::TimeDelta last_first_pts;
  while (last_first_pts < base::Seconds(1)) {
    scoped_refptr<DecoderBuffer> buffer = ReadBuffer(first_stream);
    ASSERT_NE(buffer, nullptr);
    ASSERT_FALSE(buffer->end_of_stream());
    last_first_pts = buffer->timestamp();
  }

  constexpr base::TimeDelta kSwitchTime = base::Milliseconds(200);
  MediaTrack::Id second_track_id = media_tracks_->tracks()[1]->track_id();
  base::test::TestFuture<DemuxerStream*> switch_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, second_track_id, kSwitchTime,
                            switch_future.GetCallback());
  ASSERT_EQ(switch_future.Get(), second_stream);
  EXPECT_FALSE(first_stream->is_enabled());
  EXPECT_TRUE(second_stream->is_enabled());

  // WebM `SimpleBlock`s carry zero duration, so `MkvReader` lands on the
  // nearest packet around `kSwitchTime` (~215 ms) rather than continuing past
  // `last_first_pts` (>1 s).
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(second_stream);
  ASSERT_NE(buffer, nullptr);
  ASSERT_FALSE(buffer->end_of_stream());
  EXPECT_GE(buffer->timestamp(), kSwitchTime - base::Milliseconds(50));
  EXPECT_LT(buffer->timestamp(), base::Milliseconds(300));
}

TEST_F(SymphoniaDemuxerTest, ReenablingTrackAtEndOfStreamEndsStream) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();
  ASSERT_NE(media_tracks_, nullptr);
  MediaTrack::Id track_id = media_tracks_->tracks()[0]->track_id();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  ExpectReadsAllPackets();

  base::test::TestFuture<DemuxerStream*> disable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, std::nullopt,
                            stream->duration(), disable_future.GetCallback());
  ASSERT_EQ(disable_future.Get(), nullptr);

  // Re-enabling past the last packet hits `OutOfRange` and must immediately
  // mark the stream at end of stream rather than stalling or logging an error.
  base::test::TestFuture<DemuxerStream*> enable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, track_id,
                            stream->duration() + base::Seconds(1),
                            enable_future.GetCallback());
  ASSERT_EQ(enable_future.Get(), stream);

  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_TRUE(buffer->end_of_stream());
}

TEST_F(SymphoniaDemuxerTest, AbortPendingReadsCompletesHeldTrackChange) {
  CreateDemuxer("multitrack-2audio.webm");
  InitializeDemuxer();
  ASSERT_NE(media_tracks_, nullptr);
  MediaTrack::Id track_id = media_tracks_->tracks()[0]->track_id();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);

  base::test::TestFuture<DemuxerStream*> disable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, std::nullopt,
                            base::TimeDelta(), disable_future.GetCallback());
  ASSERT_EQ(disable_future.Get(), nullptr);
  base::ThreadPoolInstance::Get()->FlushForTesting();

  base::test::TestFuture<void> read_held;
  file_data_source_->BlockReads(read_held.GetSequenceBoundCallback());

  base::test::TestFuture<DemuxerStream*> enable_future;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, track_id,
                            base::Milliseconds(500),
                            enable_future.GetCallback());
  ASSERT_TRUE(read_held.Wait());
  EXPECT_FALSE(enable_future.IsReady());

  demuxer_->AbortPendingReads();
  EXPECT_TRUE(enable_future.IsReady());
  EXPECT_EQ(enable_future.Get(), stream);

  // Subsequent seeks and reads must still work after aborting a track change.
  Seek(kSeekTarget);
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  EXPECT_FALSE(buffer->end_of_stream());
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
  CreateDemuxer("multitrack-2audio.webm");
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
  EXPECT_EQ(media_tracks_->tracks()[0]->track_id(), MediaTrack::Id("1"));
  EXPECT_EQ(media_tracks_->tracks()[1]->track_id(), MediaTrack::Id("2"));
  EXPECT_EQ(demuxer_->GetContainerForMetrics(),
            container_names::MediaContainerName::kContainerWEBM);

  // Packets for the disabled track are dropped rather than buffered.
  ExpectReadsAllPackets();
  EXPECT_EQ(disabled_stream->GetMemoryUsage(), 0);
}

TEST_F(SymphoniaDemuxerTest, NumbersTracksFromOne) {
  // Ogg identifies tracks by stream serial number. Like `FFmpegDemuxer`, the
  // track is reported as track 1 instead.
  CreateDemuxer("sfx.ogg");
  InitializeDemuxer();

  ASSERT_NE(media_tracks_, nullptr);
  ASSERT_EQ(media_tracks_->tracks().size(), 1u);
  EXPECT_EQ(media_tracks_->tracks()[0]->track_id(), MediaTrack::Id("1"));
  EXPECT_EQ(media_tracks_->tracks()[0]->stream_id(), 1);

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->media_track_id(), MediaTrack::Id("1"));

  // Track changes use the same ID.
  base::test::TestFuture<DemuxerStream*> enabled;
  demuxer_->OnTracksChanged(DemuxerStream::AUDIO, MediaTrack::Id("1"),
                            base::TimeDelta(), enabled.GetCallback());
  EXPECT_EQ(enabled.Get(), stream);
}

TEST_F(SymphoniaDemuxerTest, UnknownDurationIsSetAtEndOfStream) {
  // Without a file size, like an HTTP response with no Content-Length header,
  // Symphonia can't estimate the duration of an MP3 without a Xing header.
  auto data_source = std::make_unique<UnknownSizeFileDataSource>();
  ASSERT_TRUE(
      data_source->Initialize(GetTestDataFilePath("silent-section.mp3")));
  CreateDemuxerForDataSource(std::move(data_source));
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());
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

TEST_F(SymphoniaDemuxerTest, ShortKnownDurationIsRaisedAsPacketsArrive) {
  std::string flac;
  ASSERT_TRUE(base::ReadFileToString(GetTestDataFilePath("sfx.flac"), &flac));
  ASSERT_GT(flac.size(), 26u);
  ASSERT_EQ(flac.substr(0, 4), "fLaC");

  // Patch the 36-bit `total_samples` field at the end of bytes [18..26] in the
  // STREAMINFO block down from 12,719 (~288 ms) to 100 (~2.27 ms), so
  // `Initialize()` reports a short finite duration that reading packets raises.
  flac[21] &= 0xF0;
  flac[22] = 0;
  flac[23] = 0;
  flac[24] = 0;
  flac[25] = 100;
  CreateDemuxerWithContents(flac);

  std::vector<base::TimeDelta> reported_durations;
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());
  EXPECT_CALL(host_, SetDuration(_))
      .WillRepeatedly([&](base::TimeDelta duration) {
        reported_durations.push_back(duration);
      });

  base::test::TestFuture<PipelineStatus> status;
  demuxer_->Initialize(&host_, status.GetCallback());
  EXPECT_EQ(status.Get(), PIPELINE_OK);

  ExpectReadsAllPackets();

  // Initial duration is ~2.27 ms (100 / 44,100 s), then packets at 4,608 and
  // 9,216 samples raise it past 200 ms monotonically.
  ASSERT_GE(reported_durations.size(), 2u);
  EXPECT_LT(reported_durations.front(), base::Milliseconds(5));
  EXPECT_GT(reported_durations.back(), base::Milliseconds(200));
  EXPECT_TRUE(std::ranges::is_sorted(reported_durations));
}

TEST_F(SymphoniaDemuxerTest, UnknownDurationHasUnknownLiveness) {
  auto data_source = std::make_unique<UnknownSizeFileDataSource>();
  ASSERT_TRUE(
      data_source->Initialize(GetTestDataFilePath("silent-section.mp3")));
  CreateDemuxerForDataSource(std::move(data_source));
  InitializeDemuxer();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->liveness(), StreamLiveness::kUnknown);
}

TEST_F(SymphoniaDemuxerTest, KnownDurationIsRecordedAndSetsBitrate) {
  auto data_source = std::make_unique<BitrateRecordingFileDataSource>();
  ASSERT_TRUE(data_source->Initialize(GetTestDataFilePath("sfx_s16le.wav")));
  BitrateRecordingFileDataSource* data_source_ptr = data_source.get();
  CreateDemuxerForDataSource(std::move(data_source));
  InitializeDemuxer();
  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  EXPECT_EQ(stream->liveness(), StreamLiveness::kRecorded);

  // sfx_s16le.wav is 44.1 kHz mono 16-bit PCM, so about 705.6 kbps.
  EXPECT_GT(data_source_ptr->bitrate(), 700'000);
  EXPECT_LT(data_source_ptr->bitrate(), 720'000);
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
      base::DoNothing(), media_log_.Clone(), /*allow_fallback=*/true);

  EXPECT_CALL(host_, SetDuration(_));
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_)).Times(AnyNumber());
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

TEST_F(SymphoniaDemuxerTest, PopulatesOpusCodecDelayAndSeekPreroll) {
  CreateDemuxer("bear-opus.ogg");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.codec(), AudioCodec::kOpus);
  EXPECT_GT(config.codec_delay(), 0);
  EXPECT_EQ(config.seek_preroll(), base::Milliseconds(80));
}

TEST_F(SymphoniaDemuxerTest, PopulatesOpusWebmCodecDelayAndSeekPreroll) {
  CreateDemuxer("bear-opus.webm");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.codec(), AudioCodec::kOpus);
  EXPECT_GT(config.codec_delay(), 0);
  EXPECT_EQ(config.seek_preroll(), base::Milliseconds(80));
}

TEST_F(SymphoniaDemuxerTest, OpusSeekAppliesSeekPreroll) {
  CreateDemuxer("bear-opus.ogg");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  ASSERT_EQ(stream->audio_decoder_config().seek_preroll(),
            base::Milliseconds(80));

  constexpr base::TimeDelta kOpusSeekTarget = base::Milliseconds(500);
  Seek(kOpusSeekTarget);
  scoped_refptr<DecoderBuffer> buffer = ReadBuffer(stream);
  ASSERT_NE(buffer, nullptr);
  ASSERT_FALSE(buffer->end_of_stream());
  EXPECT_LE(buffer->timestamp(),
            kOpusSeekTarget - stream->audio_decoder_config().seek_preroll());
}

TEST_F(SymphoniaDemuxerTest, UnmaskedFourChannelWavUsesQuadLayout) {
  CreateDemuxer("4ch.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.channels(), 4);
  EXPECT_EQ(config.channel_layout(), CHANNEL_LAYOUT_QUAD);
}

TEST_F(SymphoniaDemuxerTest, SupportsDiscreteMultichannelAudio) {
  CreateDemuxer("9ch.wav");
  InitializeDemuxer();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  AudioDecoderConfig config = stream->audio_decoder_config();
  EXPECT_EQ(config.channels(), 9);
  EXPECT_EQ(config.channel_layout(), CHANNEL_LAYOUT_DISCRETE);
}

TEST_F(SymphoniaDemuxerTest, ReportsBufferedTimeRanges) {
  CreateDemuxer("sfx_s16le.wav");
  InitializeDemuxer();

  Ranges<base::TimeDelta> last_ranges;
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_))
      .WillRepeatedly(
          [&](const Ranges<base::TimeDelta>& ranges) { last_ranges = ranges; });

  ExpectReadsAllPackets();

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  ASSERT_EQ(last_ranges.size(), 1u);
  EXPECT_EQ(last_ranges.start(0), base::TimeDelta());
  EXPECT_GE(last_ranges.end(0), stream->duration());
}

TEST_F(SymphoniaDemuxerTest, SeekLeavesSkippedMiddleUnbuffered) {
  CreateDemuxer("silent-section.mp3");
  Ranges<base::TimeDelta> last_ranges;
  EXPECT_CALL(host_, SetDuration(_)).Times(AnyNumber());
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_))
      .WillRepeatedly(
          [&](const Ranges<base::TimeDelta>& ranges) { last_ranges = ranges; });
  base::test::TestFuture<PipelineStatus> status;
  demuxer_->Initialize(&host_, status.GetCallback());
  ASSERT_EQ(status.Get(), PIPELINE_OK);

  SymphoniaDemuxerStream* stream = GetStream(DemuxerStream::AUDIO);
  ASSERT_NE(stream, nullptr);
  scoped_refptr<DecoderBuffer> first = ReadBuffer(stream);
  ASSERT_NE(first, nullptr);
  ASSERT_FALSE(first->end_of_stream());

  // Jump near the end of the ~14.4s file and read to EOS; the skipped middle
  // must not be reported as buffered.
  Seek(base::Seconds(13));
  ExpectReadsAllPackets();

  ASSERT_EQ(last_ranges.size(), 2u);
  EXPECT_LT(last_ranges.end(0), base::Seconds(5));
  EXPECT_GT(last_ranges.start(1), base::Seconds(12));
}

namespace {

// Sets the state of `kSymphoniaDemuxing`, along with the WebM and WAV
// demuxing features when enabled. Other containers, like FLAC, fall back to
// FFmpegDemuxer. Used as a base class so the state is set before
// `TaskEnvironment` starts threads that may read features.
class ScopedSymphoniaDemuxingState {
 protected:
  explicit ScopedSymphoniaDemuxingState(bool enabled) {
    if (enabled) {
      feature_list_.InitWithFeatures(
          {kSymphoniaDemuxing, kSymphoniaMkvDemuxing, kSymphoniaRiffDemuxing},
          {});
    } else {
      feature_list_.InitAndDisableFeature(kSymphoniaDemuxing);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(ENABLE_FFMPEG)
constexpr bool kSymphoniaDemuxingStates[] = {true, false};
#else
// Without FFmpeg, no demuxer is available when Symphonia is disabled.
constexpr bool kSymphoniaDemuxingStates[] = {true};
#endif  // BUILDFLAG(ENABLE_FFMPEG)

}  // namespace

// Shared setup for tests of `DemuxerManager` demuxer selection. Like in
// `WebMediaPlayerImpl`, the demuxer is created on the main thread and driven on
// a separate media sequence.
class SymphoniaDemuxerManagerTestBase : public ScopedSymphoniaDemuxingState {
 protected:
  explicit SymphoniaDemuxerManagerTestBase(bool symphonia_enabled)
      : ScopedSymphoniaDemuxingState(symphonia_enabled) {}

  // Creates `manager_`, which reads `filename`, and returns it.
  DemuxerManager& CreateManager(std::string_view filename = "sfx_s16le.wav") {
    manager_ = std::make_unique<DemuxerManager>(&client_, url::Origin(),
                                                media_task_runner_, &media_log_,
                                                /*demuxer_override=*/nullptr);
    auto data_source = std::make_unique<BlockingFileDataSource>();
    CHECK(data_source->Initialize(GetTestDataFilePath(filename)));
    data_source_ = data_source.get();
    manager_->SetDataSource(std::move(data_source));
    return *manager_;
  }

  // Has `manager` create a demuxer for progressive playback, and returns it.
  Demuxer* CreateDemuxer(DemuxerManager& manager) {
    Demuxer* demuxer = nullptr;
    PipelineStatus status = manager.CreateDemuxer(
        /*load_media_source=*/false, DataSource::METADATA,
        /*needs_first_frame=*/false,
        base::BindLambdaForTesting(
            [&](Demuxer* created_demuxer, Pipeline::StartType, bool, bool) {
              demuxer = created_demuxer;
              return PipelineStatus(PIPELINE_OK);
            }));
    CHECK_EQ(status, PIPELINE_OK);
    CHECK(demuxer);
    return demuxer;
  }

  PipelineStatus InitializeOnMediaSequence(Demuxer* demuxer) {
    base::test::TestFuture<PipelineStatus> status;
    media_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&Demuxer::Initialize, base::Unretained(demuxer), &host_,
                       status.GetSequenceBoundCallback()));
    return status.Take();
  }

  void StopOnMediaSequence(Demuxer* demuxer) {
    base::test::TestFuture<void> stopped;
    media_task_runner_->PostTaskAndReply(
        FROM_HERE, base::BindOnce(&Demuxer::Stop, base::Unretained(demuxer)),
        stopped.GetCallback());
    ASSERT_TRUE(stopped.Wait());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  NiceMock<MockMediaLog> media_log_;
  NiceMock<MockDemuxerHost> host_;
  NiceMock<MockDemuxerManagerClient> client_;
  std::unique_ptr<DemuxerManager> manager_;
  // The data source made by `CreateManager()`, owned by `manager_`.
  raw_ptr<BlockingFileDataSource> data_source_ = nullptr;
};

// Tests `DemuxerManager` demuxer selection with `kSymphoniaDemuxing` enabled
// (param true) or disabled (param false).
class SymphoniaDemuxerManagerTest : public testing::TestWithParam<bool>,
                                    public SymphoniaDemuxerManagerTestBase {
 public:
  SymphoniaDemuxerManagerTest() : SymphoniaDemuxerManagerTestBase(GetParam()) {}

 protected:
  // Creates a demuxer for `filename`, checks its type, initializes it on the
  // media sequence, waits for `tracks_added_`, and stops it. Expectations on
  // `client_` must be set first.
  void CreateInitializeAndStopDemuxer(std::string_view filename) {
    DemuxerManager& manager = CreateManager(filename);
    Demuxer* demuxer = CreateDemuxer(manager);
    EXPECT_EQ(manager.GetDemuxerType(), GetParam()
                                            ? DemuxerType::kSymphoniaDemuxer
                                            : DemuxerType::kFFmpegDemuxer);
    EXPECT_EQ(InitializeOnMediaSequence(demuxer), PIPELINE_OK);

    // The demuxer's tracks reach the client through `DemuxerManager` on the
    // main thread.
    tracks_added_.Run();

    StopOnMediaSequence(demuxer);
  }

  // Quit once the expected `AddTrack()` calls have happened.
  base::RunLoop tracks_added_;
};

TEST_P(SymphoniaDemuxerManagerTest, CreatesAndInitializesDemuxer) {
  EXPECT_CALL(client_,
              AddTrack(Property(&MediaTrack::type, MediaTrack::Type::kAudio)))
      .WillOnce(RunClosure(tracks_added_.QuitClosure()));
  CreateInitializeAndStopDemuxer("sfx_s16le.wav");
}

TEST_P(SymphoniaDemuxerManagerTest, AddsEveryTrackOfMultiTrackFile) {
  constexpr int kAudioTracks = 2;
  base::RepeatingClosure track_added =
      base::BarrierClosure(kAudioTracks, tracks_added_.QuitClosure());
  EXPECT_CALL(client_,
              AddTrack(Property(&MediaTrack::type, MediaTrack::Type::kAudio)))
      .Times(kAudioTracks)
      .WillRepeatedly(RunClosure(track_added));
  CreateInitializeAndStopDemuxer("multitrack-2audio.webm");
}

INSTANTIATE_TEST_SUITE_P(All,
                         SymphoniaDemuxerManagerTest,
                         testing::ValuesIn(kSymphoniaDemuxingStates),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "SymphoniaEnabled"
                                             : "SymphoniaDisabled";
                         });

#if BUILDFLAG(ENABLE_FFMPEG)
// Tests the fallback to FFmpegDemuxer for a container that Symphonia demuxing
// is not enabled for.
class SymphoniaDemuxerManagerFallbackTest
    : public testing::Test,
      public SymphoniaDemuxerManagerTestBase {
 public:
  SymphoniaDemuxerManagerFallbackTest()
      : SymphoniaDemuxerManagerTestBase(/*symphonia_enabled=*/true) {}
};

TEST_F(SymphoniaDemuxerManagerFallbackTest, FallsBackToFFmpegDemuxer) {
  DemuxerManager& manager = CreateManager("sfx.flac");
  Demuxer* demuxer = CreateDemuxer(manager);
  ASSERT_EQ(manager.GetDemuxerType(), DemuxerType::kSymphoniaDemuxer);

  PipelineStatus status = InitializeOnMediaSequence(demuxer);
  EXPECT_EQ(status, DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Like the pipeline, stop the demuxer on the media sequence when the client
  // stops for the demuxer reset.
  EXPECT_CALL(client_, StopForDemuxerReset()).WillOnce([&] {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Demuxer::Stop, base::Unretained(demuxer)));
  });
  base::test::TestFuture<void> restarted;
  EXPECT_CALL(client_, RestartForDemuxerChange(/*is_hls=*/false))
      .WillOnce(RunClosure(restarted.GetRepeatingCallback()));
  manager.OnPipelineError(std::move(status));
  ASSERT_TRUE(restarted.Wait());
  EXPECT_FALSE(manager.GetDemuxerType().has_value());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Like `WebMediaPlayerImpl::RestartForDemuxerChange()`, create the demuxer
  // again. FFmpegDemuxer reads the data source that SymphoniaDemuxer released
  // without stopping.
  base::test::TestFuture<void> track_added;
  EXPECT_CALL(client_,
              AddTrack(Property(&MediaTrack::type, MediaTrack::Type::kAudio)))
      .WillOnce(RunClosure(track_added.GetRepeatingCallback()));
  demuxer = CreateDemuxer(manager);
  EXPECT_EQ(manager.GetDemuxerType(), DemuxerType::kFFmpegDemuxer);
  EXPECT_FALSE(data_source_->stopped());
  EXPECT_EQ(InitializeOnMediaSequence(demuxer), PIPELINE_OK);
  ASSERT_TRUE(track_added.Wait());

  StopOnMediaSequence(demuxer);
}

TEST_F(SymphoniaDemuxerManagerFallbackTest,
       StopsDataSourceWhenFallbackIsDisallowed) {
  DemuxerManager& manager = CreateManager("sfx.flac");
  Demuxer* demuxer = CreateDemuxer(manager);
  PipelineStatus status = InitializeOnMediaSequence(demuxer);
  ASSERT_EQ(status, DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  StopOnMediaSequence(demuxer);
  EXPECT_FALSE(data_source_->stopped());
  testing::Mock::VerifyAndClearExpectations(&client_);

  // SymphoniaDemuxer released the data source without stopping it. With no
  // fallback, DemuxerManager reports the error and stops the data source.
  EXPECT_CALL(client_,
              OnError(PipelineStatus(DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED)));
  EXPECT_CALL(client_, RestartForDemuxerChange(_)).Times(0);
  manager.DisallowFallback();
  manager.OnPipelineError(std::move(status));
  EXPECT_TRUE(data_source_->stopped());
}

#if BUILDFLAG(ENABLE_HLS_DEMUXER)
// An HLS manifest without an .m3u8 extension can fall back twice: from
// SymphoniaDemuxer to FFmpegDemuxer, which detects HLS, and then to the HLS
// demuxer.
TEST_F(SymphoniaDemuxerManagerFallbackTest, FallsBackTwiceForHlsManifest) {
  DemuxerManager& manager = CreateManager("hls/mp_ts_avc1.hls");
  Demuxer* demuxer = CreateDemuxer(manager);
  ASSERT_EQ(manager.GetDemuxerType(), DemuxerType::kSymphoniaDemuxer);
  PipelineStatus status = InitializeOnMediaSequence(demuxer);
  ASSERT_EQ(status, DEMUXER_ERROR_SYMPHONIA_UNSUPPORTED);
  testing::Mock::VerifyAndClearExpectations(&client_);

  // Fall back from SymphoniaDemuxer to FFmpegDemuxer, keeping the data source.
  EXPECT_CALL(client_, StopForDemuxerReset()).WillOnce([&] {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Demuxer::Stop, base::Unretained(demuxer)));
  });
  base::test::TestFuture<void> restarted_for_ffmpeg;
  EXPECT_CALL(client_, RestartForDemuxerChange(/*is_hls=*/false))
      .WillOnce(RunClosure(restarted_for_ffmpeg.GetRepeatingCallback()));
  manager.OnPipelineError(std::move(status));
  ASSERT_TRUE(restarted_for_ffmpeg.Wait());
  testing::Mock::VerifyAndClearExpectations(&client_);

  demuxer = CreateDemuxer(manager);
  ASSERT_EQ(manager.GetDemuxerType(), DemuxerType::kFFmpegDemuxer);
  status = InitializeOnMediaSequence(demuxer);
  ASSERT_EQ(status, DEMUXER_ERROR_DETECTED_HLS);
  EXPECT_FALSE(data_source_->stopped());

  // Fall back from FFmpegDemuxer to the HLS demuxer, which frees the data
  // source.
  EXPECT_CALL(client_, StopForDemuxerReset()).WillOnce([&] {
    media_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&Demuxer::Stop, base::Unretained(demuxer)));
  });
  base::test::TestFuture<void> restarted_for_hls;
  EXPECT_CALL(client_, RestartForDemuxerChange(/*is_hls=*/true))
      .WillOnce(RunClosure(restarted_for_hls.GetRepeatingCallback()));
  data_source_ = nullptr;
  manager.OnPipelineError(std::move(status));
  ASSERT_TRUE(restarted_for_hls.Wait());
  testing::Mock::VerifyAndClearExpectations(&client_);

  CreateDemuxer(manager);
  EXPECT_EQ(manager.GetDemuxerType(), DemuxerType::kManifestDemuxer);
}
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)
#endif  // BUILDFLAG(ENABLE_FFMPEG)

}  // namespace media

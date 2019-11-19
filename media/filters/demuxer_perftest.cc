// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/media.h"
#include "media/base/media_tracks.h"
#include "media/base/media_util.h"
#include "media/base/test_data_util.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "media/filters/file_data_source.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace media {

static const int kBenchmarkIterations = 100;

class DemuxerHostImpl : public media::DemuxerHost {
 public:
  DemuxerHostImpl() = default;
  ~DemuxerHostImpl() override = default;

  // DemuxerHost implementation.
  void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) override {}
  void SetDuration(base::TimeDelta duration) override {}
  void OnDemuxerError(media::PipelineStatus error) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DemuxerHostImpl);
};

static void QuitLoopWithStatus(base::Closure quit_cb,
                               media::PipelineStatus status) {
  CHECK_EQ(status, media::PIPELINE_OK);
  quit_cb.Run();
}

static void OnEncryptedMediaInitData(EmeInitDataType init_data_type,
                                     const std::vector<uint8_t>& init_data) {
  DVLOG(1) << "File is encrypted.";
}

static void OnMediaTracksUpdated(std::unique_ptr<MediaTracks> tracks) {
  DVLOG(1) << "Got media tracks info, tracks = " << tracks->tracks().size();
}

typedef std::vector<media::DemuxerStream*> Streams;

// Simulates playback reading requirements by reading from each stream
// present in |demuxer| in as-close-to-monotonically-increasing timestamp order.
class StreamReader {
 public:
  StreamReader(media::Demuxer* demuxer, bool enable_bitstream_converter);
  ~StreamReader();

  // Performs a single step read.
  void Read();

  // Returns true when all streams have reached end of stream.
  bool IsDone();

  int number_of_streams() { return static_cast<int>(streams_.size()); }
  const Streams& streams() { return streams_; }
  const std::vector<int>& counts() { return counts_; }

 private:
  void OnReadDone(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  const base::Closure& quit_when_idle_closure,
                  bool* end_of_stream,
                  base::TimeDelta* timestamp,
                  media::DemuxerStream::Status status,
                  scoped_refptr<DecoderBuffer> buffer);
  int GetNextStreamIndexToRead();

  Streams streams_;
  std::vector<bool> end_of_stream_;
  std::vector<base::TimeDelta> last_read_timestamp_;
  std::vector<int> counts_;

  DISALLOW_COPY_AND_ASSIGN(StreamReader);
};

StreamReader::StreamReader(media::Demuxer* demuxer,
                           bool enable_bitstream_converter) {
  std::vector<media::DemuxerStream*> streams = demuxer->GetAllStreams();
  for (auto* stream : streams) {
    streams_.push_back(stream);
    end_of_stream_.push_back(false);
    last_read_timestamp_.push_back(media::kNoTimestamp);
    counts_.push_back(0);
    if (enable_bitstream_converter && stream->type() == DemuxerStream::VIDEO)
      stream->EnableBitstreamConverter();
  }
}

StreamReader::~StreamReader() = default;

void StreamReader::Read() {
  int index = GetNextStreamIndexToRead();
  bool end_of_stream = false;
  base::TimeDelta timestamp;

  base::RunLoop run_loop;
  streams_[index]->Read(base::BindOnce(
      &StreamReader::OnReadDone, base::Unretained(this),
      base::ThreadTaskRunnerHandle::Get(), run_loop.QuitWhenIdleClosure(),
      &end_of_stream, &timestamp));
  run_loop.Run();

  CHECK(end_of_stream || timestamp != media::kNoTimestamp);
  end_of_stream_[index] = end_of_stream;
  last_read_timestamp_[index] = timestamp;
  counts_[index]++;
}

bool StreamReader::IsDone() {
  for (size_t i = 0; i < end_of_stream_.size(); ++i) {
    if (!end_of_stream_[i])
      return false;
  }
  return true;
}

void StreamReader::OnReadDone(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::Closure& quit_when_idle_closure,
    bool* end_of_stream,
    base::TimeDelta* timestamp,
    media::DemuxerStream::Status status,
    scoped_refptr<DecoderBuffer> buffer) {
  CHECK_EQ(status, media::DemuxerStream::kOk);
  CHECK(buffer);
  *end_of_stream = buffer->end_of_stream();
  *timestamp = *end_of_stream ? media::kNoTimestamp : buffer->timestamp();
  task_runner->PostTask(FROM_HERE, quit_when_idle_closure);
}

int StreamReader::GetNextStreamIndexToRead() {
  int index = -1;
  for (int i = 0; i < number_of_streams(); ++i) {
    // Ignore streams at EOS.
    if (end_of_stream_[i])
      continue;

    // Use a stream if it hasn't been read from yet.
    if (last_read_timestamp_[i] == media::kNoTimestamp)
      return i;

    if (index < 0 || last_read_timestamp_[i] < last_read_timestamp_[index]) {
      index = i;
    }
  }
  CHECK_GE(index, 0) << "Couldn't find a stream to read";
  return index;
}

static void RunDemuxerBenchmark(const std::string& filename) {
  base::FilePath file_path(GetTestDataFilePath(filename));
  base::TimeDelta total_time;
  NullMediaLog media_log_;
  for (int i = 0; i < kBenchmarkIterations; ++i) {
    // Setup.
    base::test::TaskEnvironment task_environment_;
    DemuxerHostImpl demuxer_host;
    FileDataSource data_source;
    ASSERT_TRUE(data_source.Initialize(file_path));

    Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb =
        base::BindRepeating(&OnEncryptedMediaInitData);
    Demuxer::MediaTracksUpdatedCB tracks_updated_cb =
        base::Bind(&OnMediaTracksUpdated);
    FFmpegDemuxer demuxer(base::ThreadTaskRunnerHandle::Get(), &data_source,
                          encrypted_media_init_data_cb, tracks_updated_cb,
                          &media_log_, true);

    {
      base::RunLoop run_loop;
      demuxer.Initialize(&demuxer_host, base::Bind(&QuitLoopWithStatus,
                                                   run_loop.QuitClosure()));
      run_loop.Run();
    }

    StreamReader stream_reader(&demuxer, false);

    // Benchmark.
    base::TimeTicks start = base::TimeTicks::Now();
    while (!stream_reader.IsDone())
      stream_reader.Read();
    total_time += base::TimeTicks::Now() - start;
    demuxer.Stop();
    base::RunLoop().RunUntilIdle();
  }

  perf_test::PerfResultReporter reporter("demuxer_bench", filename);
  reporter.RegisterImportantMetric("", "runs/s");
  reporter.AddResult("", kBenchmarkIterations / total_time.InSecondsF());
}

class DemuxerPerfTest : public testing::TestWithParam<const char*> {};

TEST_P(DemuxerPerfTest, Demuxer) {
  RunDemuxerBenchmark(GetParam());
}

static const char* kDemuxerTestFiles[] {
  "bear.ogv", "bear-640x360.webm", "sfx.mp3",
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      "bear-1280x720.mp4",
#endif
};

// For simplicity we only test containers with above 2% daily usage as measured
// by the Media.DetectedContainer histogram.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    DemuxerPerfTest,
    testing::ValuesIn(kDemuxerTestFiles));

}  // namespace media

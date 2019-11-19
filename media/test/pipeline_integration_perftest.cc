// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"
#include "media/media_buildflags.h"
#include "media/test/pipeline_integration_test_base.h"
#include "testing/perf/perf_result_reporter.h"

namespace media {

static const int kBenchmarkIterationsAudio = 200;
static const int kBenchmarkIterationsVideo = 20;

static void RunPlaybackBenchmark(const std::string& filename,
                                 const std::string& metric_suffix,
                                 int iterations,
                                 bool audio_only) {
  double time_seconds = 0.0;

  for (int i = 0; i < iterations; ++i) {
    PipelineIntegrationTestBase pipeline;

    ASSERT_EQ(PIPELINE_OK, pipeline.Start(filename));

    base::TimeTicks start = base::TimeTicks::Now();
    pipeline.Play();

    ASSERT_TRUE(pipeline.WaitUntilOnEnded());

    // Call Stop() to ensure that the rendering is complete.
    pipeline.Stop();

    if (audio_only) {
      time_seconds += pipeline.GetAudioTime().InSecondsF();
    } else {
      time_seconds += (base::TimeTicks::Now() - start).InSecondsF();
    }
  }

  perf_test::PerfResultReporter reporter("clockless_video_playback", filename);
  reporter.RegisterImportantMetric(metric_suffix, "runs/s");
  reporter.AddResult(metric_suffix, iterations / time_seconds);
}

static void RunVideoPlaybackBenchmark(const std::string& filename,
                                      const std::string name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsVideo, false);
}

static void RunAudioPlaybackBenchmark(const std::string& filename,
                                      const std::string& name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsAudio, true);
}

class ClocklessAudioPipelineIntegrationPerfTest
    : public testing::TestWithParam<const char*> {};

TEST_P(ClocklessAudioPipelineIntegrationPerfTest, PlaybackBenchmark) {
  RunAudioPlaybackBenchmark(GetParam(), "clockless_playback");
}

static const char* kAudioTestFiles[] {
  "sfx_s16le.wav", "sfx.ogg", "sfx.mp3",
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
      "sfx.m4a",
#endif
};

// For simplicity we only test codecs with above 2% daily usage as measured by
// the Media.AudioCodec histogram.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ClocklessAudioPipelineIntegrationPerfTest,
    testing::ValuesIn(kAudioTestFiles));

TEST(PipelineIntegrationPerfTest, VP8PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear_silent.webm", "_vp8");
}

TEST(PipelineIntegrationPerfTest, VP9PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-vp9.webm", "_vp9");
}

#if BUILDFLAG(ENABLE_AV1_DECODER)
TEST(PipelineIntegrationPerfTest, AV1PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-av1-640x480.webm", "_av1");
}
#endif

#if BUILDFLAG(USE_PROPRIETARY_CODECS) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
TEST(PipelineIntegrationPerfTest, MP4PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear_silent.mp4", "_mp4");
}
#endif

}  // namespace media

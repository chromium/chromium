// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/web_media_player_ms_compositor.h"

#include <array>
#include <initializer_list>
#include <memory>
#include <numeric>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_video_track.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;

constexpr double ComputeHarmonicFrameRate(
    base::span<const size_t> durations_ms) {
  size_t sum = std::accumulate(durations_ms.begin(), durations_ms.end(), 0);
  size_t sum_squares = std::accumulate(
      durations_ms.begin(), durations_ms.end(), 0,
      [](size_t acc, size_t time) { return acc + time * time; });
  return round(1000.0 * sum / sum_squares);
}

constexpr double ComputeReproductionJitter(
    base::span<const std::pair<int32_t, int32_t>>
        display_and_capture_durations_ms) {
  double sum_squares = 0;
  for (const auto& [display_duration_ms, capture_duration_ms] :
       display_and_capture_durations_ms) {
    int32_t error_ms = display_duration_ms - capture_duration_ms;
    sum_squares += error_ms * error_ms;
  }
  return round(sqrt(sum_squares / display_and_capture_durations_ms.size()));
}

class ExtendedMockMediaStreamVideoSource : public MockMediaStreamVideoSource {
 public:
  void SetHasSeenScreencastContentTypeCallback(
      base::OnceClosure callback) override {
    has_seen_screencast_content_type_callback_ = std::move(callback);
  }

  void CallContentTypeScreenshareCallback(base::OnceClosure quit_closure) {
    if (has_seen_screencast_content_type_callback_) {
      std::move(has_seen_screencast_content_type_callback_).Run();
    }
    std::move(quit_closure).Run();
  }

 private:
  base::OnceClosure has_seen_screencast_content_type_callback_;
};

class WebMediaPlayerMSCompositorTest : public testing::Test {
 public:
  static constexpr double TENX = 10;
  static constexpr size_t kRtpFrequencyKilohertz = 90;
  static constexpr char k10xHarmonicFramerateHistogramName[] =
      "Media.WebMediaPlayerCompositor.10xHarmonicFrameRate";
  static constexpr char kReproductionJitterVideoCaptureHistogramName[] =
      "Media.WebMediaPlayerCompositor.ReproductionJitter.Unspecified.Capture";
  static constexpr char kReproductionJitterVideoRemoteHistogramName[] =
      "Media.WebMediaPlayerCompositor.ReproductionJitter.Unspecified.Remote";
  static constexpr char kReproductionJitterScreencastCaptureHistogramName[] =
      "Media.WebMediaPlayerCompositor.ReproductionJitter.Screencast.Capture";
  static constexpr char kReproductionJitterScreencastRemoteHistogramName[] =
      "Media.WebMediaPlayerCompositor.ReproductionJitter.Screencast.Remote";

  struct Timestamps {
    int32_t display_time_ms;
    std::optional<int32_t> capture_begin_time_ms = std::nullopt;
    std::optional<uint32_t> rtp_timestamp = std::nullopt;
  };

  enum class ContentType {
    kVideo,
    kScreencast,
  };

  ~WebMediaPlayerMSCompositorTest() override {
    compositor_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void Initialize(ContentType content_type) {
    bool is_initially_screencast = content_type == ContentType::kScreencast;
    auto mock_source = std::make_unique<ExtendedMockMediaStreamVideoSource>();
    mock_source_ptr_ = mock_source.get();
    source_ = MakeGarbageCollected<MediaStreamSource>(
        "source_id", MediaStreamSource::kTypeVideo, "source_name",
        /*remote=*/false, std::move(mock_source));
    WebMediaStreamTrack web_track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_ptr_, VideoTrackAdapterSettings(),
        /*noise_reduction=*/std::nullopt, is_initially_screencast,
        /*min_frame_rate=*/std::nullopt,
        /*image_capture_device_settings=*/nullptr,
        /*pan_tilt_zoom_allowed=*/false, base::DoNothing(),
        /*enabled=*/true);
    MediaStreamComponent* component = web_track;
    auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
        MediaStreamComponentVector{}, MediaStreamComponentVector{component});
    compositor_ = std::make_unique<WebMediaPlayerMSCompositor>(
        main_thread_, main_thread_, descriptor, nullptr, false, nullptr);
  }

  void PresentFrames(base::span<const Timestamps> timestamps) {
    for (const auto& timestamp : timestamps) {
      std::optional<base::TimeTicks> capture_begin_time;
      if (timestamp.capture_begin_time_ms.has_value()) {
        capture_begin_time =
            base::TimeTicks() +
            base::Milliseconds(timestamp.capture_begin_time_ms.value());
      }
      compositor_->OnFramePresented(
          base::TimeTicks() + base::Milliseconds(timestamp.display_time_ms),
          capture_begin_time, timestamp.rtp_timestamp);
    }
  }

  void ConcludeHistograms() {
    compositor_ = nullptr;
    mock_source_ptr_ = nullptr;
  }

  void CallContentTypeScreenshareCallback(base::RunLoop& run_loop) {
    mock_source_ptr_->CallContentTypeScreenshareCallback(
        base::BindPostTask(main_thread_, run_loop.QuitClosure()));
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

 private:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_{
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()};
  raw_ptr<ExtendedMockMediaStreamVideoSource> mock_source_ptr_ = nullptr;
  Persistent<MediaStreamSource> source_;
  std::unique_ptr<WebMediaPlayerMSCompositor> compositor_;
};

TEST_F(WebMediaPlayerMSCompositorTest, EmitsHarmonicFramerate) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  PresentFrames({{0}, {20}, {30}});
  double hfps = ComputeHarmonicFrameRate({20, 10});
  ConcludeHistograms();
  tester.ExpectUniqueSample(k10xHarmonicFramerateHistogramName, TENX * hfps, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest, EmitsHarmonicFramerateWithPause) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  // We insert a pause causing the compositor to forget the last sample at 20 ms
  // and having to restart.
  PresentFrames({{0}, {20}, {2020}, {2023}});
  double hfps = ComputeHarmonicFrameRate({20, 3});  // 38.5
  ConcludeHistograms();
  tester.ExpectUniqueSample(k10xHarmonicFramerateHistogramName, TENX * hfps, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest,
       EmitsReproJitterAndHfpsForCapturedVideo) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  PresentFrames({{0, 0},  //
                 {20, 25}});
  double repro_jitter = ComputeReproductionJitter({{20, 25}});
  ConcludeHistograms();
  EXPECT_THAT(tester.GetAllSamples(k10xHarmonicFramerateHistogramName),
              Not(IsEmpty()));
  tester.ExpectUniqueSample(kReproductionJitterVideoCaptureHistogramName,
                            repro_jitter, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest, EmitsReproJitterAndHfpsForRemoteVideo) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  PresentFrames({{0, std::nullopt, 0},  //
                 {20, std::nullopt, kRtpFrequencyKilohertz * 25}});
  double repro_jitter = ComputeReproductionJitter({{20, 25}});
  ConcludeHistograms();
  EXPECT_THAT(tester.GetAllSamples(k10xHarmonicFramerateHistogramName),
              Not(IsEmpty()));
  tester.ExpectUniqueSample(kReproductionJitterVideoRemoteHistogramName,
                            repro_jitter, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest,
       EmitsReproJitterAndHfpsForCapturedScreencast) {
  Initialize(ContentType::kScreencast);
  base::HistogramTester tester;
  PresentFrames({{0, 0},  //
                 {20, 25}});
  double repro_jitter = ComputeReproductionJitter({{20, 25}});
  ConcludeHistograms();
  // We omit hFps for screencast.
  EXPECT_THAT(tester.GetAllSamples(k10xHarmonicFramerateHistogramName),
              IsEmpty());
  tester.ExpectUniqueSample(kReproductionJitterScreencastCaptureHistogramName,
                            repro_jitter, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest,
       EmitsReproJitterAndHfpsForRemoteScreencast) {
  Initialize(ContentType::kScreencast);
  base::HistogramTester tester;
  PresentFrames(
      {{0, std::nullopt, 0}, {20, std::nullopt, kRtpFrequencyKilohertz * 25}});
  double repro_jitter = ComputeReproductionJitter({{20, 25}});
  ConcludeHistograms();
  // We omit hFps for screencast.
  EXPECT_THAT(tester.GetAllSamples(k10xHarmonicFramerateHistogramName),
              IsEmpty());
  tester.ExpectUniqueSample(kReproductionJitterScreencastRemoteHistogramName,
                            repro_jitter, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest,
       EmitsReproJitterCrossCaptureRemotePrefersRemote) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  PresentFrames({{0, 0, std::nullopt},  //
                 {10, 17, kRtpFrequencyKilohertz * 10},
                 {20, std::nullopt, kRtpFrequencyKilohertz * 25}});
  double repro_jitter_remote = ComputeReproductionJitter({{10, 15}});
  ConcludeHistograms();
  EXPECT_THAT(
      tester.GetAllSamples(kReproductionJitterVideoCaptureHistogramName),
      IsEmpty());
  tester.ExpectUniqueSample(kReproductionJitterVideoRemoteHistogramName,
                            repro_jitter_remote, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest, EmitsReproJitterWithPause) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  // We insert a pause causing the compositor to forget the last sample at 20 ms
  // and having to restart.
  PresentFrames({{0, 0},  //
                 {10, 13},
                 {2020, 2020},
                 {2030, 2034}});
  double repro_jitter = ComputeReproductionJitter({{10, 13}, {10, 14}});
  ConcludeHistograms();
  tester.ExpectUniqueSample(kReproductionJitterVideoCaptureHistogramName,
                            repro_jitter, 1);
}

TEST_F(WebMediaPlayerMSCompositorTest, OmitsHfpsWhenScreenshareNotedLater) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  PresentFrames({{0, 0},  //
                 {20, 25}});

  // Calling the content type callback will mark the track as a screencast track
  // - which will cause the compositor to omit hFps.
  base::RunLoop run_loop;
  CallContentTypeScreenshareCallback(run_loop);
  run_loop.Run();
  ConcludeHistograms();
  EXPECT_THAT(tester.GetAllSamples(k10xHarmonicFramerateHistogramName),
              IsEmpty());
}

TEST_F(WebMediaPlayerMSCompositorTest, PeriodicallyEmitsMetrics) {
  Initialize(ContentType::kVideo);
  base::HistogramTester tester;
  // 1 Duration with 50 hFps and 1 in reproduction jitter.
  PresentFrames({{0, 0},  //
                 {20, 21}});
  FastForwardBy(WebMediaPlayerMSCompositor::kMetricsTimerInterval);
  // There's no relation between the metrics timer time base and the frame time
  // base w.r.t the metrics so we can expect metrics to be emitted again with
  // only slightly higher timestamp values despite real time being advanced
  // beyond sample timestamps. However we must stay above the 2-second reset
  // interval for the estimators.
  //
  // 1 Duration with 25 hFps and 5 in reproduction jitter.
  PresentFrames({{3000, 3000},  //
                 {3040, 3045}});
  ConcludeHistograms();

  EXPECT_EQ(tester.GetTotalSum(k10xHarmonicFramerateHistogramName),
            10 * (50 + 25));
  EXPECT_EQ(tester.GetTotalSum(kReproductionJitterVideoCaptureHistogramName),
            1 + 5);
}
}  // namespace
}  // namespace blink

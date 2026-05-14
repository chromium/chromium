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
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "cc/layers/video_frame_provider.h"
#include "media/base/video_frame.h"
#include "media/base/video_transformation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_video_frame_submitter.h"
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

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Return;

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

class MockWebVideoFrameSubmitter : public WebVideoFrameSubmitter {
 public:
  MOCK_METHOD2(Initialize, void(cc::VideoFrameProvider*, bool));
  MOCK_METHOD0(StartRendering, void());
  MOCK_METHOD0(StopRendering, void());
  MOCK_METHOD0(StopUsingProvider, void());
  MOCK_METHOD0(DidReceiveFrame, void());
  MOCK_METHOD1(EnableSubmission, void(viz::SurfaceId));
  MOCK_METHOD1(SetTransform, void(media::VideoTransformation));
  MOCK_METHOD1(SetIsSurfaceVisible, void(bool));
  MOCK_METHOD1(SetIsPageVisible, void(bool));
  MOCK_METHOD1(SetForceSubmit, void(bool));
  MOCK_METHOD1(SetForceBeginFrames, void(bool));
  MOCK_CONST_METHOD0(IsDrivingFrameUpdates, bool());
  MOCK_CONST_METHOD0(GetExpectedDisplayTime, std::optional<base::TimeTicks>());
};

// Tests that WebMediaPlayerMSCompositor wires GetExpectedDisplayTime() from the
// submitter into the expected_display_time field of the video frame metadata.
class WebMediaPlayerMSCompositorSubmitterTest : public testing::Test {
 public:
  ~WebMediaPlayerMSCompositorSubmitterTest() override {
    mock_submitter_ = nullptr;
    compositor_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  void Initialize(bool render_with_algorithm) {
    auto mock_source = std::make_unique<ExtendedMockMediaStreamVideoSource>();
    mock_source_ptr_ = mock_source.get();
    source_ = MakeGarbageCollected<MediaStreamSource>(
        "source_id", MediaStreamSource::kTypeVideo, "source_name",
        /*remote=*/false, std::move(mock_source));
    WebMediaStreamTrack web_track = MediaStreamVideoTrack::CreateVideoTrack(
        mock_source_ptr_, VideoTrackAdapterSettings(),
        /*noise_reduction=*/std::nullopt, /*is_screencast=*/false,
        /*min_frame_rate=*/std::nullopt,
        /*image_capture_device_settings=*/nullptr,
        /*pan_tilt_zoom_allowed=*/false, base::DoNothing(),
        /*enabled=*/true);
    MediaStreamComponent* component = web_track;
    auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
        MediaStreamComponentVector{}, MediaStreamComponentVector{component});

    auto submitter =
        std::make_unique<testing::NiceMock<MockWebVideoFrameSubmitter>>();
    mock_submitter_ = submitter.get();

    // Wait for compositor thread initialization to complete.
    base::RunLoop init_loop;
    EXPECT_CALL(*mock_submitter_, Initialize(_, _))
        .WillOnce(
            [&init_loop](cc::VideoFrameProvider*, bool) { init_loop.Quit(); });

    compositor_ = std::make_unique<WebMediaPlayerMSCompositor>(
        main_thread_, main_thread_, descriptor, std::move(submitter),
        /*use_surface_layer=*/true, nullptr);
    if (render_with_algorithm) {
      compositor_->SetAlgorithmEnabledForTesting(true);
    }
    init_loop.Run();
  }

  scoped_refptr<media::VideoFrame> CreateTestFrame() {
    return media::VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  }

  // Polls the compositor until UpdateCurrentFrame() returns true (a frame is
  // available that has not been painted yet). Avoids base::test::RunUntil(),
  // which evaluates the predicate from an on-next-idle callback inside the
  // sequence manager and can post work in a state where
  // CheckedLock::AssertNoLockHeldOnCurrentThread() fails on some configurations
  // (flaky DCHECK on Android x86 bots with MOCK_TIME). Avoids
  // TaskEnvironment::RunUntilIdle(), which Blink presubmit discourages; use the
  // same RunLoop drain pattern as elsewhere in this file.
  bool PollUntilFrameNeedsPaint(base::TimeTicks deadline_min,
                                base::TimeTicks deadline_max) {
    constexpr int kMaxRounds = 100;
    for (int i = 0; i < kMaxRounds; ++i) {
      base::RunLoop drain;
      main_thread_->PostTask(FROM_HERE, drain.QuitClosure());
      drain.Run();
      if (compositor_->UpdateCurrentFrame(deadline_min, deadline_max)) {
        return true;
      }
    }
    return false;
  }

  void TestNulloptFallbackProducesValidExpectedDisplayTime(
      bool render_with_algorithm) {
    Initialize(render_with_algorithm);

    EXPECT_CALL(*mock_submitter_, GetExpectedDisplayTime())
        .WillRepeatedly(Return(std::nullopt));

    if (!render_with_algorithm) {
      compositor_->EnqueueFrame(CreateTestFrame(), /*is_copy=*/false);
      base::RunLoop drain;
      main_thread_->PostTask(FROM_HERE, drain.QuitClosure());
      drain.Run();
    } else {
      compositor_->StartRendering();
      const base::TimeDelta kStep = base::Milliseconds(16);
      const base::TimeTicks now = base::TimeTicks::Now();
      // Poll until rendering has started; the first successful call also primes
      // the deadline state needed by EnqueueFrame.
      ASSERT_TRUE(PollUntilFrameNeedsPaint(now, now + kStep));
      auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
      frame->metadata().reference_time = now + kStep;
      compositor_->EnqueueFrame(std::move(frame), /*is_copy=*/false);
      // Drive a render cycle to pick up the enqueued frame.
      compositor_->UpdateCurrentFrame(now + kStep, now + kStep * 2);
    }

    auto metadata = compositor_->GetLastPresentedFrameMetadata();
    EXPECT_FALSE(metadata->expected_display_time.is_null());
  }

 protected:
  test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_{
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()};
  raw_ptr<ExtendedMockMediaStreamVideoSource> mock_source_ptr_ = nullptr;
  Persistent<MediaStreamSource> source_;
  std::unique_ptr<WebMediaPlayerMSCompositor> compositor_;
  raw_ptr<testing::NiceMock<MockWebVideoFrameSubmitter>> mock_submitter_ =
      nullptr;
};

// The submitter's GetExpectedDisplayTime() estimate must reach the frame
// metadata when the rendering algorithm is disabled.
TEST_F(WebMediaPlayerMSCompositorSubmitterTest,
       ExpectedDisplayTimeFromSubmitterUsedForNoAlgorithmPath) {
  Initialize(/*render_with_algorithm=*/false);

  const base::TimeTicks adaptive_time =
      base::TimeTicks() + base::Milliseconds(500);
  EXPECT_CALL(*mock_submitter_, GetExpectedDisplayTime())
      .WillRepeatedly(Return(std::optional<base::TimeTicks>(adaptive_time)));

  compositor_->EnqueueFrame(CreateTestFrame(), /*is_copy=*/false);
  {
    base::RunLoop drain;
    main_thread_->PostTask(FROM_HERE, drain.QuitClosure());
    drain.Run();
  }

  auto metadata = compositor_->GetLastPresentedFrameMetadata();
  EXPECT_EQ(metadata->expected_display_time, adaptive_time);
}

// When GetExpectedDisplayTime() returns nullopt, the compositor must still
// produce a valid (non-null) expected_display_time in the frame metadata,
// for both rendering paths.
TEST_F(WebMediaPlayerMSCompositorSubmitterTest,
       FallsBackWhenGetExpectedDisplayTimeReturnsNulloptNoAlgorithmPath) {
  TestNulloptFallbackProducesValidExpectedDisplayTime(
      /*render_with_algorithm=*/false);
}

TEST_F(WebMediaPlayerMSCompositorSubmitterTest,
       FallsBackWhenGetExpectedDisplayTimeReturnsNulloptAlgorithmPath) {
  TestNulloptFallbackProducesValidExpectedDisplayTime(
      /*render_with_algorithm=*/true);
}

// The submitter's GetExpectedDisplayTime() estimate must reach the frame
// metadata when the rendering algorithm is enabled.
TEST_F(WebMediaPlayerMSCompositorSubmitterTest,
       ExpectedDisplayTimeFromSubmitterUsedForAlgorithmPath) {
  Initialize(/*render_with_algorithm=*/true);

  compositor_->StartRendering();

  const base::TimeDelta kStep = base::Milliseconds(16);
  const base::TimeTicks now = base::TimeTicks::Now();

  // Poll until rendering has started; the first successful call also primes
  // the deadline state needed by EnqueueFrame.
  ASSERT_TRUE(PollUntilFrameNeedsPaint(now, now + kStep));

  const base::TimeTicks adaptive_time = now + base::Milliseconds(500);
  EXPECT_CALL(*mock_submitter_, GetExpectedDisplayTime())
      .WillRepeatedly(Return(std::optional<base::TimeTicks>(adaptive_time)));

  auto frame = media::VideoFrame::CreateBlackFrame(gfx::Size(8, 8));
  frame->metadata().reference_time = now + kStep;
  compositor_->EnqueueFrame(std::move(frame), /*is_copy=*/false);

  // Drive a render cycle to pick up the enqueued frame.
  compositor_->UpdateCurrentFrame(now + kStep, now + kStep * 2);

  auto metadata = compositor_->GetLastPresentedFrameMetadata();
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(metadata->expected_display_time, adaptive_time);
}

}  // namespace
}  // namespace blink

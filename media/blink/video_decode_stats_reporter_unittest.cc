// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/blink/video_decode_stats_reporter.h"
#include "media/capabilities/bucket_utility.h"
#include "media/mojo/interfaces/video_decode_stats_recorder.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace media {

const VideoCodec kDefaultCodec = kCodecVP9;
const VideoCodecProfile kDefaultProfile = VP9PROFILE_PROFILE0;
const int kDefaultHeight = 480;
const int kDefaultWidth = 640;
const double kDefaultFps = 30;
const int kDecodeCountIncrement = 20;
const int kDroppedCountIncrement = 1;
const int kDecodePowerEfficientCountIncrement = 1;

VideoDecoderConfig MakeVideoConfig(VideoCodec codec,
                                   VideoCodecProfile profile,
                                   gfx::Size natural_size) {
  gfx::Size coded_size = natural_size;
  gfx::Rect visible_rect(coded_size.width(), coded_size.height());
  return VideoDecoderConfig(codec, profile, PIXEL_FORMAT_I420, COLOR_SPACE_JPEG,
                            VIDEO_ROTATION_0, coded_size, visible_rect,
                            natural_size, EmptyExtraData(), Unencrypted());
}

VideoDecoderConfig MakeDefaultVideoConfig() {
  return MakeVideoConfig(kDefaultCodec, kDefaultProfile,
                         gfx::Size(kDefaultWidth, kDefaultHeight));
}

PipelineStatistics MakeStats(int frames_decoded,
                             int frames_dropped,
                             int power_efficient_decoded_frames,
                             double fps) {
  // Will initialize members with reasonable defaults.
  PipelineStatistics stats;
  stats.video_frames_decoded = frames_decoded;
  stats.video_frames_dropped = frames_dropped;
  stats.video_frames_decoded_power_efficient = power_efficient_decoded_frames;
  stats.video_frame_duration_average = base::TimeDelta::FromSecondsD(1.0 / fps);
  return stats;
}

// Mock VideoDecodeStatsRecorder to verify reporter/recorder interactions.
class RecordInterceptor : public mojom::VideoDecodeStatsRecorder {
 public:
  RecordInterceptor() = default;
  ~RecordInterceptor() override = default;

  // Until move-only types work.
  void StartNewRecord(mojom::PredictionFeaturesPtr features) override {
    MockStartNewRecord(features->profile, features->video_size,
                       features->frames_per_sec);
  }

  MOCK_METHOD3(MockStartNewRecord,
               void(VideoCodecProfile profile,
                    const gfx::Size& natural_size,
                    int frames_per_sec));

  void UpdateRecord(mojom::PredictionTargetsPtr targets) override {
    MockUpdateRecord(targets->frames_decoded, targets->frames_dropped,
                     targets->frames_power_efficient);
  }

  MOCK_METHOD3(MockUpdateRecord,
               void(uint32_t frames_decoded,
                    uint32_t frames_dropped,
                    uint32_t frames_power_efficient));

  MOCK_METHOD0(FinalizeRecord, void());
};

class VideoDecodeStatsReporterTest : public ::testing::Test {
 public:
  VideoDecodeStatsReporterTest()
      : kDefaultSize_(kDefaultWidth, kDefaultHeight) {}
  ~VideoDecodeStatsReporterTest() override = default;

  void SetUp() override {
    // Do this first. Lots of pieces depend on the task runner.
    auto message_loop = base::MessageLoopCurrent::Get();
    original_task_runner_ = message_loop.task_runner();
    task_runner_ = new base::TestMockTimeTaskRunner();
    message_loop.SetTaskRunner(task_runner_);

    // Make reporter with default configuration. Connects RecordInterceptor as
    // remote mojo VideoDecodeStatsRecorder.
    MakeReporter();

    // Start each test with no decodes, no drops, and steady framerate.
    pipeline_decoded_frames_ = 0;
    pipeline_dropped_frames_ = 0;
    pipeline_decoded_power_efficient_frames_ = 0;
    pipeline_framerate_ = kDefaultFps;
  }

  void TearDown() override {
    // Break the IPC connection if reporter still around.
    reporter_.reset();

    // Run task runner to have Mojo cleanup interceptor_.
    task_runner_->RunUntilIdle();
    base::MessageLoopCurrent::Get().SetTaskRunner(original_task_runner_);
  }

  PipelineStatistics MakeAdvancingDecodeStats() {
    pipeline_decoded_frames_ += kDecodeCountIncrement;
    pipeline_dropped_frames_ += kDroppedCountIncrement;
    pipeline_decoded_power_efficient_frames_ +=
        kDecodePowerEfficientCountIncrement;
    return MakeStats(pipeline_decoded_frames_, pipeline_dropped_frames_,
                     pipeline_decoded_power_efficient_frames_,
                     pipeline_framerate_);
  }

  // Peek at what MakeAdvancingDecodeStats() will return next without advancing
  // the tracked counts.
  PipelineStatistics PeekNextDecodeStats() const {
    return MakeStats(pipeline_decoded_frames_ + kDecodeCountIncrement,
                     pipeline_dropped_frames_ + kDroppedCountIncrement,
                     pipeline_decoded_power_efficient_frames_ +
                         kDecodePowerEfficientCountIncrement,
                     pipeline_framerate_);
  }

 protected:
  // Indicates expectations about rate of timer firing during FPS stabilization.
  enum FpsStabiliaztionSpeed {
    // Timer is expected to fire at rate of 3 * last observed average frame
    // duration. This is the default for framerate detection.
    FAST_STABILIZE_FPS,
    // Timer is expected to fire at a rate of kRecordingInterval. This will
    // occur
    // when decode progress stalls during framerate detection.
    SLOW_STABILIZE_FPS,
  };

  // Bind the RecordInterceptor to the request for a VideoDecodeStatsRecorder.
  // The interceptor serves as a mock recorder to verify reporter/recorder
  // interactions.
  void SetupRecordInterceptor(mojom::VideoDecodeStatsRecorderPtr* recorder_ptr,
                              RecordInterceptor** interceptor) {
    // Capture a the interceptor pointer for verifying recorder calls. Lifetime
    // will be managed by the |recorder_ptr|.
    *interceptor = new RecordInterceptor();

    // Bind interceptor as the VideoDecodeStatsRecorder.
    mojom::VideoDecodeStatsRecorderRequest request =
        mojo::MakeRequest(recorder_ptr);
    mojo::MakeStrongBinding(base::WrapUnique(*interceptor),
                            mojo::MakeRequest(recorder_ptr));
    EXPECT_TRUE(recorder_ptr->is_bound());
  }

  // Inject mock objects and create a new |reporter_| to test.
  void MakeReporter() {
    mojom::VideoDecodeStatsRecorderPtr recorder_ptr;
    SetupRecordInterceptor(&recorder_ptr, &interceptor_);

    reporter_ = std::make_unique<VideoDecodeStatsReporter>(
        std::move(recorder_ptr),
        base::Bind(&VideoDecodeStatsReporterTest::GetPipelineStatsCB,
                   base::Unretained(this)),
        MakeDefaultVideoConfig(), task_runner_,
        task_runner_->GetMockTickClock());
  }

  // Fast forward the task runner (and associated tick clock) by |milliseconds|.
  void FastForward(base::TimeDelta delta) {
    task_runner_->FastForwardBy(delta);
  }

  bool ShouldBeReporting() const { return reporter_->ShouldBeReporting(); }

  const VideoDecoderConfig& CurrentVideoConfig() const {
    return reporter_->video_config_;
  }

  base::TimeDelta CurrentStatsCbInterval() const {
    return reporter_->stats_cb_timer_.GetCurrentDelay();
  }

  int CurrentStableFpsSamples() const {
    return reporter_->num_stable_fps_samples_;
  }

  // Call at the start of tests to stabilize framerate.
  // Preconditions:
  //  1) Reporter should not already be in playing state
  //  2) Playing should unblock the reporter to begin reporting (e.g. not in
  //     hidden state)
  //  3) No progress made yet toward stabilizing framerate.
  void StartPlayingAndStabilizeFramerate() {
    DCHECK(!reporter_->is_playing_);
    DCHECK_EQ(reporter_->num_stable_fps_samples_, 0);

    // Setup stats callback to provide steadily advancing decode stats with a
    // constant framerate.
    ON_CALL(*this, GetPipelineStatsCB())
        .WillByDefault(Invoke(
            this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

    // On playing should start timer at recording interval. Expect first stats
    // CB when that interval has elapsed.
    reporter_->OnPlaying();
    DCHECK(ShouldBeReporting());
    EXPECT_CALL(*this, GetPipelineStatsCB());
    FastForward(kRecordingInterval);

    StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                        kDefaultFps);
  }

  // Call just after detecting a change to framerate. |profile|, |natural_size|,
  // and |frames_per_sec| should match the call to StartNewRecord(...) once the
  // framerate is stabilized. |fps_timer_speed| indicates the expected timer
  // interval to be used during stabilization (see FpsStabiliaztionSpeed
  // definition).
  // Preconditions:
  //  1. Do not call if framerate already stable (know what you're testing).
  //  2. Only call with GetPipelineStatsCB configured to return
  //     progressing decode stats with a steady framerate.
  void StabilizeFramerateAndStartNewRecord(
      VideoCodecProfile profile,
      gfx::Size natural_size,
      int frames_per_sec,
      FpsStabiliaztionSpeed fps_timer_speed = FAST_STABILIZE_FPS) {
    base::TimeDelta last_frame_duration = kNoTimestamp;
    uint32_t last_decoded_frames = 0;

    while (CurrentStableFpsSamples() < kRequiredStableFpsSamples) {
      PipelineStatistics next_stats = PeekNextDecodeStats();

      // Sanity check that the stats callback is progressing decode.
      DCHECK_GT(next_stats.video_frames_decoded, last_decoded_frames);
      last_decoded_frames = next_stats.video_frames_decoded;

      // Sanity check that the stats callback is providing steady fps.
      if (last_frame_duration != kNoTimestamp) {
        DCHECK_EQ(next_stats.video_frame_duration_average, last_frame_duration);
      }
      last_frame_duration = next_stats.video_frame_duration_average;

      // The final iteration stabilizes framerate and starts a new record.
      if (CurrentStableFpsSamples() == kRequiredStableFpsSamples - 1) {
        EXPECT_CALL(*interceptor_,
                    MockStartNewRecord(profile, natural_size, frames_per_sec));
      }

      if (fps_timer_speed == FAST_STABILIZE_FPS) {
        // Generally FPS is stabilized with a timer of ~3x the average frame
        // duration.
        base::TimeDelta frame_druation =
            base::TimeDelta::FromSecondsD(1.0 / pipeline_framerate_);
        EXPECT_EQ(CurrentStatsCbInterval(), frame_druation * 3);
      } else {
        // If the playback is struggling we will do it more slowly to avoid
        // firing high frequency timers indefinitely. Store constant as a local
        // to workaround linking confusion.
        DCHECK_EQ(fps_timer_speed, SLOW_STABILIZE_FPS);

        // fixme
        EXPECT_EQ(CurrentStatsCbInterval(), kRecordingInterval);
      }

      EXPECT_CALL(*this, GetPipelineStatsCB());
      FastForward(CurrentStatsCbInterval());
    }
  }

  // Advances the task runner by a single recording interval and verifies that
  // the record is updated. The values provided to UpdateRecord(...)
  // should match values from PeekNextDecodeStates(...), minus the offsets of
  // |decoded_frames_offset|, |dropped_frames_offset| and
  // |decoded_power_efficient_offset|.
  // Preconditions:
  // - Should only be called during regular reporting (framerate stable,
  //   not in background, not paused).
  void AdvanceTimeAndVerifyRecordUpdate(int decoded_frames_offset,
                                        int dropped_frames_offset,
                                        int decoded_power_efficient_offset) {
    DCHECK(ShouldBeReporting());

    // Record updates should always occur at recording interval. Store to local
    // variable to workaround linker confusion with test macros.
    EXPECT_EQ(kRecordingInterval, CurrentStatsCbInterval());

    PipelineStatistics next_stats = PeekNextDecodeStats();

    // Decode stats must be advancing for record updates to be expected. Dropped
    // frames should at least not move backward.
    EXPECT_GT(next_stats.video_frames_decoded, pipeline_decoded_frames_);
    EXPECT_GT(next_stats.video_frames_decoded_power_efficient,
              pipeline_decoded_power_efficient_frames_);
    EXPECT_GE(next_stats.video_frames_dropped, pipeline_dropped_frames_);

    // Verify that UpdateRecord calls come at the recording interval with
    // correct values.
    EXPECT_CALL(*this, GetPipelineStatsCB());
    EXPECT_CALL(*interceptor_,
                MockUpdateRecord(
                    next_stats.video_frames_decoded - decoded_frames_offset,
                    next_stats.video_frames_dropped - dropped_frames_offset,
                    next_stats.video_frames_decoded_power_efficient -
                        decoded_power_efficient_offset));
    FastForward(kRecordingInterval);
  }

  // Injected callback for fetching statistics. Each test will manage
  // expectations and return behavior.
  MOCK_METHOD0(GetPipelineStatsCB, PipelineStatistics());

  // These track the last values returned by MakeAdvancingDecodeStats(). See
  // SetUp() for initialization.
  uint32_t pipeline_decoded_frames_;
  uint32_t pipeline_dropped_frames_;
  uint32_t pipeline_decoded_power_efficient_frames_;
  double pipeline_framerate_;

  // Placed as a class member to avoid static initialization costs.
  const gfx::Size kDefaultSize_;

  // Task runner that allows for manual advancing of time. Instantiated during
  // Setup(). |original_task_runner_| is a copy of the TaskRunner in place prior
  // to the start of this test. It's restored after the test completes.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> original_task_runner_;

  // Points to the interceptor that acts as a VideoDecodeStatsRecorder. The
  // object is owned by VideoDecodeStatsRecorderPtr, which is itself owned by
  // |reporter_|.
  RecordInterceptor* interceptor_ = nullptr;

  // The VideoDecodeStatsReporter being tested.
  std::unique_ptr<VideoDecodeStatsReporter> reporter_;

  // Redefined constants in fixture for easy access from tests. See
  // video_decode_stats_reporter.h for documentation.
  static const base::TimeDelta kRecordingInterval;
  static const base::TimeDelta kTinyFpsWindowDuration;
  static const int kRequiredStableFpsSamples =
      VideoDecodeStatsReporter::kRequiredStableFpsSamples;
  static const int kMaxUnstableFpsChanges =
      VideoDecodeStatsReporter::kMaxUnstableFpsChanges;
  static const int kMaxTinyFpsWindows =
      VideoDecodeStatsReporter::kMaxTinyFpsWindows;
};

const base::TimeDelta VideoDecodeStatsReporterTest::kRecordingInterval =
    base::TimeDelta::FromMilliseconds(
        VideoDecodeStatsReporter::kRecordingIntervalMs);
const base::TimeDelta VideoDecodeStatsReporterTest::kTinyFpsWindowDuration =
    base::TimeDelta::FromMilliseconds(
        VideoDecodeStatsReporter::kTinyFpsWindowMs);

TEST_F(VideoDecodeStatsReporterTest, RecordWhilePlaying) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Once more for good measure.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, RecordingStopsWhenPaused) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // When paused, expect no stats callbacks and no record updates.
  reporter_->OnPaused();
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  // Advance a few recording intervals just to be sure.
  FastForward(kRecordingInterval * 3);

  // Verify callbacks and record updates resume when playing again. No changes
  // to the stream during pause, so no need to re-stabilize framerate. Offsets
  // for stats count are still valid.
  reporter_->OnPlaying();
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, RecordingStopsWhenHidden) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // When hidden, expect no stats callbacks and no record updates.
  reporter_->OnHidden();
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  // Advance a few recording intervals just to be sure.
  FastForward(kRecordingInterval * 3);

  // Verify updates resume when visible again. Dropped frame stats are not valid
  // while hidden, so expect a new record to begin. GetPipelineStatsCB will be
  // called to update offsets to ignore stats while hidden.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  EXPECT_CALL(*interceptor_,
              MockStartNewRecord(kDefaultProfile, kDefaultSize_, kDefaultFps));
  reporter_->OnShown();

  // Update offsets for new record and verify updates resume as time advances.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, RecordingStopsWhenNoDecodeProgress) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Freeze decode stats at current values, simulating network underflow.
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Return(MakeStats(
          pipeline_decoded_frames_, pipeline_dropped_frames_,
          pipeline_decoded_power_efficient_frames_, pipeline_framerate_)));

  // Verify record updates stop while decode is not progressing. Fast forward
  // through several recording intervals to be sure we never call UpdateRecord.
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(3);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Resume progressing decode!
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Invoke(
          this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

  // Verify record updates resume.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, NewRecordStartsForSizeChange) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Change the natural size.
  const gfx::Size size_720p(1280, 720);
  reporter_->OnNaturalSizeChanged(size_720p);

  // Next stats update will not cause a record update. We must first check
  // to see if the framerate changes and start a new record.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval);

  // A new record is started with the latest natural size as soon as the
  // framerate is confirmed (re-stabilized).
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, size_720p, kDefaultFps);

  // Offsets should be adjusted so the new record starts at zero.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;

  // Stats callbacks and record updates should proceed as usual.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, NewRecordStartsForConfigChange) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Change the config to use profile 2.
  auto new_config =
      MakeVideoConfig(kCodecVP9, VP9PROFILE_PROFILE2, kDefaultSize_);
  EXPECT_FALSE(new_config.Matches(CurrentVideoConfig()));
  reporter_->OnVideoConfigChanged(new_config);

  // Next stats update will not cause a record update. We must first check
  // to see if the framerate changes and start a new record.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval);

  // A new record is started with the latest configuration as soon as the
  // framerate is confirmed (re-stabilized).
  StabilizeFramerateAndStartNewRecord(VP9PROFILE_PROFILE2, kDefaultSize_,
                                      kDefaultFps);

  // Offsets should be adjusted so the new record starts at zero.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;

  // Stats callbacks and record updates should proceed as usual.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, NewRecordStartsForFpsChange) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Change FPS to 2x current rate. Future calls to GetPipelineStats will
  // use this to compute frame duration.
  EXPECT_EQ(pipeline_framerate_, static_cast<uint32_t>(kDefaultFps));
  pipeline_framerate_ *= 2;

  // Next stats update will not cause a record update. It will instead begin
  // detection of the new framerate.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval);

  // A new record is started with the latest frames per second as soon as the
  // framerate is confirmed (re-stabilized).
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                      kDefaultFps * 2);

  // Offsets should be adjusted so the new record starts at zero.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;

  // Stats callbacks and record updates should proceed as usual.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, FpsStabilizationFailed) {
  // Setup stats callback to provide steadily advancing decode stats with a
  // constant framerate.
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Invoke(
          this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

  // On playing should start timer.
  reporter_->OnPlaying();
  EXPECT_TRUE(ShouldBeReporting());

  // OnPlaying starts the timer at the recording interval. Expect first stats
  // CB when that interval has elapsed. This stats callback provides the first
  // fps sample.
  EXPECT_CALL(*this, GetPipelineStatsCB());

  // We should not start nor update a record while failing to detect fps.
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  EXPECT_CALL(*interceptor_, MockStartNewRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval);
  int num_fps_samples = 1;

  // With FPS stabilization in-progress, keep alternating framerate to thwart
  // stabilization.
  for (; num_fps_samples < kMaxUnstableFpsChanges; num_fps_samples++) {
    pipeline_framerate_ =
        (pipeline_framerate_ == kDefaultFps) ? 2 * kDefaultFps : kDefaultFps;
    EXPECT_CALL(*this, GetPipelineStatsCB());
    FastForward(CurrentStatsCbInterval());
  }

  // Stabilization has now failed, so fast forwarding  by any amount will not
  // trigger new stats update callbacks.
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  FastForward(kRecordingInterval * 10);

  // Pausing then playing does not kickstart reporting. We assume framerate is
  // still variable.
  reporter_->OnPaused();
  reporter_->OnPlaying();
  FastForward(kRecordingInterval * 10);

  // Hidden then shown does not kickstart reporting. We assume framerate is
  // still variable.
  reporter_->OnHidden();
  reporter_->OnShown();
  FastForward(kRecordingInterval * 10);

  // Unlike the above, a config change suggests the stream itself has changed so
  // we should make a new attempt at detecting a stable FPS.
  VideoDecoderConfig new_config =
      MakeVideoConfig(kDefaultCodec, VP9PROFILE_PROFILE2, kDefaultSize_);
  EXPECT_FALSE(new_config.Matches(CurrentVideoConfig()));
  reporter_->OnVideoConfigChanged(new_config);
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // With |pipeline_framerate_| holding steady, FPS should stabilize. The new
  // record should indicate we're using VP9 Profile 2.
  StabilizeFramerateAndStartNewRecord(VP9PROFILE_PROFILE2, kDefaultSize_,
                                      pipeline_framerate_);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, FpsStabilizationFailed_TinyWindows) {
  uint32_t decoded_offset = 0;
  uint32_t dropped_offset = 0;
  uint32_t decoded_power_efficient_offset = 0;

  // Setup stats callback to provide steadily advancing decode stats.
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Invoke(
          this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

  // On playing should start timer at recording interval. Expect first stats
  // CB when that interval has elapsed.
  reporter_->OnPlaying();
  DCHECK(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Repeatedly stabilize, then change the FPS after single record updates to
  // create tiny windows.
  for (int i = 0; i < kMaxTinyFpsWindows; i++) {
    StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                        pipeline_framerate_);

    // Framerate is now stable! Recorded stats should be offset by the values
    // last provided to GetPipelineStatsCB.
    decoded_offset = pipeline_decoded_frames_;
    dropped_offset = pipeline_dropped_frames_;
    decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;

    AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                     decoded_power_efficient_offset);

    // Changing the framerate and fast forward to detect the change.
    pipeline_framerate_ =
        (pipeline_framerate_ == kDefaultFps) ? 2 * kDefaultFps : kDefaultFps;
    EXPECT_CALL(*this, GetPipelineStatsCB());
    FastForward(kRecordingInterval);
  }

  // Verify no further stats updates are made because we've hit the maximum
  // number of tiny framerate windows.
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval);

  // Pausing then playing does not kickstart reporting. We assume framerate is
  // still variable.
  reporter_->OnPaused();
  reporter_->OnPlaying();
  FastForward(kRecordingInterval * 10);

  // Hidden then shown does not kickstart reporting. We assume framerate is
  // still variable.
  reporter_->OnHidden();
  reporter_->OnShown();
  FastForward(kRecordingInterval * 10);

  // Unlike the above, a natural size change suggests the stream itself has
  // changed so we should make a new attempt at detecting a stable FPS.
  gfx::Size size_720p(1280, 720);
  reporter_->OnNaturalSizeChanged(size_720p);
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // With |pipeline_framerate_| holding steady, FPS stabilization should work.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, size_720p,
                                      pipeline_framerate_);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, ThrottleFpsTimerIfNoDecodeProgress) {
  // Setup stats callback to provide steadily advancing decode stats with a
  // constant framerate.
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Invoke(
          this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

  // On playing should start timer at recording interval.
  reporter_->OnPlaying();
  EXPECT_TRUE(ShouldBeReporting());

  // OnPlaying starts the timer at the recording interval. Expect first stats
  // CB when that interval has elapsed. This stats callback provides the first
  // fps sample.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);
  int stable_fps_samples = 1;

  // Now advance time to make it half way through framerate stabilization.
  base::TimeDelta frame_duration =
      base::TimeDelta::FromSecondsD(1.0 / pipeline_framerate_);
  for (; stable_fps_samples < kRequiredStableFpsSamples / 2;
       stable_fps_samples++) {
    // The timer runs at 3x the frame duration when detecting framerate to
    // quickly stabilize.
    EXPECT_CALL(*this, GetPipelineStatsCB());
    FastForward(frame_duration * 3);
  }

  // With stabilization still ongoing, freeze decode progress by repeatedly
  // returning the same stats from before.
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Return(MakeStats(
          pipeline_decoded_frames_, pipeline_dropped_frames_,
          pipeline_decoded_power_efficient_frames_, pipeline_framerate_)));

  // Advance another fps detection interval to detect that no progress was made.
  // Verify this decreases timer frequency to standard reporting interval.
  EXPECT_LT(CurrentStatsCbInterval(), kRecordingInterval);
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(frame_duration * 3);
  EXPECT_EQ(CurrentStatsCbInterval(), kRecordingInterval);

  // Verify stats updates continue to come in at recording interval. Verify no
  // calls to UpdateRecord because decode progress is still frozen. Fast forward
  // through several recording intervals to be sure nothing changes.
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(3);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Un-freeze decode stats!
  ON_CALL(*this, GetPipelineStatsCB())
      .WillByDefault(Invoke(
          this, &VideoDecodeStatsReporterTest::MakeAdvancingDecodeStats));

  // Finish framerate stabilization with a slower timer frequency. The slower
  // timer is used to avoid firing high frequency timers indefinitely for
  // machines/networks that are struggling to keep up.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                      kDefaultFps, SLOW_STABILIZE_FPS);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, ConfigChangeStillProcessedWhenHidden) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // When hidden, expect no stats callbacks and no record updates. Advance a few
  // recording intervals just to be sure.
  reporter_->OnHidden();
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Config changes may still arrive when hidden and should not be dropped.
  // Change the config to use VP9 Profile 2.
  VideoDecoderConfig new_config =
      MakeVideoConfig(kDefaultCodec, VP9PROFILE_PROFILE2, kDefaultSize_);
  EXPECT_FALSE(new_config.Matches(CurrentVideoConfig()));
  reporter_->OnVideoConfigChanged(new_config);

  // Still, no record updates should be made until the the reporter is shown.
  FastForward(kRecordingInterval * 3);

  // When shown, the reporting timer should start running again.
  reporter_->OnShown();
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Framerate should be re-detected whenever the stream config changes. A new
  // record should be started using VP9 Profile 2 from the new config.
  StabilizeFramerateAndStartNewRecord(VP9PROFILE_PROFILE2, kDefaultSize_,
                                      kDefaultFps);

  // Update offsets for new record and verify updates resume as time advances.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, ConfigChangeStillProcessedWhenPaused) {
  StartPlayingAndStabilizeFramerate();

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Pause and verify record updates stop.
  reporter_->OnPaused();
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Config changes are still possible when paused (e.g. user seeks to a new
  // config). Change the config to VP9 Profile 2.
  auto new_config =
      MakeVideoConfig(kCodecVP9, VP9PROFILE_PROFILE2, kDefaultSize_);
  EXPECT_FALSE(new_config.Matches(CurrentVideoConfig()));
  reporter_->OnVideoConfigChanged(new_config);

  // Playback is still paused, so reporting should be stopped.
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Upon playing, expect the new config to re-trigger framerate detection and
  // to begin a new record using VP9 Profile 2. Fast forward an initial
  // recording interval to pick up the config change.
  reporter_->OnPlaying();
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);
  StabilizeFramerateAndStartNewRecord(VP9PROFILE_PROFILE2, kDefaultSize_,
                                      kDefaultFps);

  // Update offsets for new record and verify record updates.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, FpsBucketing) {
  StartPlayingAndStabilizeFramerate();
  EXPECT_EQ(kDefaultFps, pipeline_framerate_);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Small changes to framerate should not trigger a new record.
  pipeline_framerate_ = kDefaultFps + .5;
  EXPECT_CALL(*interceptor_, MockStartNewRecord(_, _, _)).Times(0);
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Small changes in the other direction should also not trigger a new record.
  pipeline_framerate_ = kDefaultFps - .5;
  EXPECT_CALL(*interceptor_, MockStartNewRecord(_, _, _)).Times(0);
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Big changes in framerate should trigger a new record.
  pipeline_framerate_ = kDefaultFps * 2;

  // Fast forward by one interval to detect framerate change.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Stabilize new framerate.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                      kDefaultFps * 2);

  // Update offsets for new record and verify recording.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Whacky framerates should be bucketed to a more common nearby value.
  pipeline_framerate_ = 123.4;

  // Fast forward by one interval to detect framerate change.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Verify new record uses bucketed framerate.
  int bucketed_fps = GetFpsBucket(pipeline_framerate_);
  EXPECT_NE(bucketed_fps, pipeline_framerate_);
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, kDefaultSize_,
                                      bucketed_fps);

  // Update offsets for new record and verify recording.
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, ResolutionBucketing) {
  StartPlayingAndStabilizeFramerate();
  EXPECT_EQ(kDefaultFps, pipeline_framerate_);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Note that our current size fits perfectly into known buckets...
  EXPECT_EQ(GetSizeBucket(kDefaultSize_), kDefaultSize_);

  // A slightly smaller size should fall into the same size bucket as before.
  gfx::Size slightly_smaller_size(kDefaultWidth - 2, kDefaultHeight - 2);
  EXPECT_EQ(GetSizeBucket(kDefaultSize_), GetSizeBucket(slightly_smaller_size));
  // Using the same bucket means we expect it continues to use the same record.
  // Verify recording progresses as if size were unchanged.
  reporter_->OnNaturalSizeChanged(slightly_smaller_size);
  EXPECT_CALL(*interceptor_, MockStartNewRecord(_, _, _)).Times(0);
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Since the original size perfectly fits a known size bucket, any small
  // increase should cause the next larger bucket should be chosen. This is done
  // to surface cut off resolutions in hardware decoders. HW acceleration can be
  // critical to smooth decode at higher resolutions.
  gfx::Size slightly_larger_size(kDefaultWidth + 1, kDefaultHeight + 1);
  gfx::Size larger_size_bucket = GetSizeBucket(slightly_larger_size);
  EXPECT_NE(GetSizeBucket(kDefaultSize_), larger_size_bucket);
  // Given that the buckets are different, a new record should be started when
  // size changes to the slightly larger value.
  reporter_->OnNaturalSizeChanged(slightly_larger_size);

  // Fast forward by one interval to detect resolution change.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Stabilize new framerate and verify record updates come with new offsets.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, larger_size_bucket,
                                      kDefaultFps);
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // With |slightly_larger_size| describing the bottom of its bucket, we should
  // have of room to increase a little further within this bucket, without
  // triggering the start of a new record.
  slightly_larger_size = gfx::Size(slightly_larger_size.width() + 1,
                                   slightly_larger_size.height() + 1);
  EXPECT_EQ(larger_size_bucket, GetSizeBucket(slightly_larger_size));
  reporter_->OnNaturalSizeChanged(slightly_larger_size);
  EXPECT_CALL(*interceptor_, MockStartNewRecord(_, _, _)).Times(0);
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Big changes in resolution should always trigger a new record.
  gfx::Size big_resolution(kDefaultWidth * 2, kDefaultHeight * 2);
  reporter_->OnNaturalSizeChanged(big_resolution);

  // Fast forward by one interval to detect resolution change.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);

  // Stabilize new framerate and verify record updates come with new offsets.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, big_resolution,
                                      kDefaultFps);
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

TEST_F(VideoDecodeStatsReporterTest, ResolutionTooSmall) {
  StartPlayingAndStabilizeFramerate();
  EXPECT_EQ(kDefaultFps, pipeline_framerate_);

  // Framerate is now stable! Recorded stats should be offset by the values
  // last provided to GetPipelineStatsCB.
  uint32_t decoded_offset = pipeline_decoded_frames_;
  uint32_t dropped_offset = pipeline_dropped_frames_;
  uint32_t decoded_power_efficient_offset =
      pipeline_decoded_power_efficient_frames_;

  // Verify that UpdateRecord calls come at the recording interval with
  // correct values.
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);

  // Change the natural size to something tiny.
  const gfx::Size tiny_size(10, 15);
  // Tiny size should "bucket" to empty.
  EXPECT_TRUE(GetSizeBucket(tiny_size).IsEmpty());
  reporter_->OnNaturalSizeChanged(tiny_size);

  // Verify reporting has stopped because because resolution is so small. Fast
  // forward through several intervals to verify no callbacks are made while the
  // tiny size is in effect.
  EXPECT_FALSE(ShouldBeReporting());
  EXPECT_CALL(*this, GetPipelineStatsCB()).Times(0);
  EXPECT_CALL(*interceptor_, MockUpdateRecord(_, _, _)).Times(0);
  FastForward(kRecordingInterval * 3);

  // Change the size to something small, but reasonable.
  const gfx::Size small_size(75, 75);
  const gfx::Size bucketed_small_size = GetSizeBucket(small_size);
  EXPECT_FALSE(bucketed_small_size.IsEmpty());
  reporter_->OnNaturalSizeChanged(small_size);

  // Fast forward by one interval to detect resolution change.
  EXPECT_CALL(*this, GetPipelineStatsCB());
  FastForward(kRecordingInterval);
  // Stabilize new framerate and verify record updates come with new offsets.
  StabilizeFramerateAndStartNewRecord(kDefaultProfile, bucketed_small_size,
                                      kDefaultFps);
  decoded_offset = pipeline_decoded_frames_;
  dropped_offset = pipeline_dropped_frames_;
  decoded_power_efficient_offset = pipeline_decoded_power_efficient_frames_;
  AdvanceTimeAndVerifyRecordUpdate(decoded_offset, dropped_offset,
                                   decoded_power_efficient_offset);
}

}  // namespace media

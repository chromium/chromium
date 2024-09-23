// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/track_audio_renderer.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/unguessable_token.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/channel_layout.h"
#include "media/base/fake_audio_renderer_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr int kSampleRate = 8000;
constexpr int kFrames = 480;
constexpr int kAltFrames = 512;
constexpr int kChannels = 2;
constexpr int kAltChannels = 1;
const media::AudioParameters kDefaultFormat(
    media::AudioParameters::Format::AUDIO_PCM_LINEAR,
    media::ChannelLayoutConfig::Stereo(),
    kSampleRate,
    kFrames);

using SinkState = media::FakeAudioRendererSink::State;

}  // namespace

// Test Platform implementation to inject an IO task runner.
class AudioRendererSinkTestingPlatformSupport
    : public IOTaskRunnerTestingPlatformSupport {
 public:
  AudioRendererSinkTestingPlatformSupport() = default;

  scoped_refptr<media::AudioRendererSink> NewAudioRendererSink(
      blink::WebAudioDeviceSourceType source_type,
      blink::WebLocalFrame* web_frame,
      const media::AudioSinkParameters& params) override {
    return fake_sink_;
  }

 private:
  scoped_refptr<media::FakeAudioRendererSink> fake_sink_ =
      base::MakeRefCounted<media::FakeAudioRendererSink>(kDefaultFormat);
};

class FakeMediaStreamAudioSource final : public MediaStreamAudioSource {
 public:
  FakeMediaStreamAudioSource()
      : MediaStreamAudioSource(scheduler::GetSingleThreadTaskRunnerForTesting(),
                               /*is_local_source=*/true) {}

  FakeMediaStreamAudioSource(const FakeMediaStreamAudioSource&) = delete;
  FakeMediaStreamAudioSource& operator=(const FakeMediaStreamAudioSource&) =
      delete;

  ~FakeMediaStreamAudioSource() override = default;

  void PushData(const media::AudioBus& data, base::TimeTicks reference_time) {
    media::ChannelLayoutConfig layout =
        data.channels() == 2 ? media::ChannelLayoutConfig::Stereo()
                             : media::ChannelLayoutConfig::Mono();

    media::AudioParameters format(
        media::AudioParameters::Format::AUDIO_PCM_LINEAR, layout, kSampleRate,
        data.frames());

    // Automatically send format changes, as a real source might.
    if (!last_format_.Equals(format)) {
      MediaStreamAudioSource::SetFormat(format);
      last_format_ = format;
    }

    MediaStreamAudioSource::DeliverDataToTracks(data, reference_time, {});
  }

 private:
  media::AudioParameters last_format_;
};

class TrackAudioRendererTest : public testing::TestWithParam<bool> {
 public:
  TrackAudioRendererTest() = default;
  ~TrackAudioRendererTest() override = default;

  void SetUp() override {
    auto source = std::make_unique<FakeMediaStreamAudioSource>();

    fake_source_ = source.get();

    auto platform_track =
        std::make_unique<MediaStreamAudioTrack>(/*is_local_track=*/true);

    auto* audio_component = MakeGarbageCollected<MediaStreamComponentImpl>(
        MakeGarbageCollected<MediaStreamSource>(
            String::FromUTF8("audio_id"), MediaStreamSource::kTypeAudio,
            String::FromUTF8("audio_track"), false /* remote */,
            std::move(source)),
        std::move(platform_track));

    static_cast<blink::MediaStreamAudioSource*>(
        audio_component->Source()->GetPlatformSource())
        ->ConnectToInitializedTrack(audio_component);

    track_renderer_ = base::MakeRefCounted<TrackAudioRenderer>(
        audio_component, dummy_page_.GetFrame(), String(),
        base::BindRepeating(&TrackAudioRendererTest::OnRenderError,
                            base::Unretained(this)));
  }

  // Simulates a stream of AudioData being captured, with inline format changes,
  // and verifies we do not drop data between reconfigurations.
  void RunCaptureIntegrationTest(
      const base::RepeatingClosure& simulate_capture_callback,
      bool expect_dropped_frames) {
    constexpr int kNumberOfCycles = 5;

    track_renderer_->Start();
    track_renderer_->Play();

    for (int i = 0; i < kNumberOfCycles; ++i) {
      simulate_capture_callback.Run();

      // The test parameter determines if we should queue many captures at once,
      // or sync between each capture.
      if (SyncAfterEachCycle())
        SyncAllSequences();
    }

    // Sync here if we haven't already.
    if (!SyncAfterEachCycle())
      SyncAllSequences();

    VerifyFrameCounts(expect_dropped_frames);

    track_renderer_->Stop();
  }

 protected:
  void VerifyFrameCounts(bool expect_dropped_frames) {
    if (expect_dropped_frames) {
      // Every frame captured since reconfiguring should have been pushed. We
      // can't verify much more than this, but at least AudioShifter should
      // DCHECK if the wrong number of channels are pushed in.
      EXPECT_GE(track_renderer_->TotalFramesPushedForTesting(),
                frames_captured_since_last_reconfig_);
      EXPECT_GE(track_renderer_->FramesInAudioShifterForTesting(),
                frames_captured_since_last_reconfig_);
    } else {
      EXPECT_EQ(track_renderer_->TotalFramesPushedForTesting(),
                total_frames_captured_);
      EXPECT_EQ(track_renderer_->FramesInAudioShifterForTesting(),
                total_frames_captured_);
    }
  }

  void SimulateDataCapture(int frames, int channels = kChannels) {
    // Sending a new number of channels will cause a reconfiguration, dropping
    // frames currently in the AudioShifter.
    if (last_channels_ != channels) {
      frames_captured_since_last_reconfig_ = 0;
      last_channels_ = channels;
    }

    // Keep track of the total number of fake frames captured.
    total_frames_captured_ += frames;
    frames_captured_since_last_reconfig_ += frames;

    IOTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&TrackAudioRendererTest::PushDataOnIO,
                                  base::Unretained(this),
                                  media::AudioBus::Create(channels, frames),
                                  base::TimeTicks::Now()));
  }

  // Force sync the IO task runner, followed by the main task runner.
  void SyncAllSequences() {
    {
      base::RunLoop loop;
      IOTaskRunner()->PostTask(FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
    {
      base::RunLoop loop;
      scheduler::GetSequencedTaskRunnerForTesting()->PostTask(
          FROM_HERE, loop.QuitClosure());
      loop.Run();
    }
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<TrackAudioRenderer> track_renderer_;

 private:
  bool SyncAfterEachCycle() { return GetParam(); }

  void OnRenderError() {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    NOTREACHED_IN_MIGRATION();
  }

  scoped_refptr<base::SingleThreadTaskRunner> IOTaskRunner() {
    return platform_->GetIOTaskRunner();
  }

  void PushDataOnIO(std::unique_ptr<media::AudioBus> data,
                    base::TimeTicks reference_time) {
    fake_source_->PushData(*data, reference_time);
  }

  THREAD_CHECKER(main_thread_checker_);

  ScopedTestingPlatformSupport<AudioRendererSinkTestingPlatformSupport>
      platform_;
  DummyPageHolder dummy_page_;

  int last_channels_ = -1;
  int total_frames_captured_ = 0;
  int frames_captured_since_last_reconfig_ = 0;

  raw_ptr<FakeMediaStreamAudioSource> fake_source_;
};

TEST_P(TrackAudioRendererTest, SingleCapture) {
  track_renderer_->Start();
  track_renderer_->Play();

  SimulateDataCapture(kFrames);

  SyncAllSequences();

  EXPECT_EQ(track_renderer_->TotalFramesPushedForTesting(), kFrames);
  EXPECT_EQ(track_renderer_->FramesInAudioShifterForTesting(), kFrames);

  track_renderer_->Stop();
}

TEST_P(TrackAudioRendererTest, Integration_IdenticalData) {
  RunCaptureIntegrationTest(
      base::BindLambdaForTesting([this]() { SimulateDataCapture(kFrames); }),
      /*expect_dropped_frames=*/false);
}

TEST_P(TrackAudioRendererTest, Integration_VariableFrames) {
  RunCaptureIntegrationTest(base::BindLambdaForTesting([this]() {
                              SimulateDataCapture(kFrames);
                              SimulateDataCapture(kAltFrames);
                            }),
                            /*expect_dropped_frames=*/false);
}

TEST_P(TrackAudioRendererTest, Integration_VariableFrames_RepeatedBuffers) {
  RunCaptureIntegrationTest(base::BindLambdaForTesting([this]() {
                              SimulateDataCapture(kFrames);
                              SimulateDataCapture(kFrames);
                              SimulateDataCapture(kAltFrames);
                              SimulateDataCapture(kAltFrames);
                            }),
                            /*expect_dropped_frames=*/false);
}

TEST_P(TrackAudioRendererTest, Integration_VariableChannels) {
  RunCaptureIntegrationTest(base::BindLambdaForTesting([this]() {
                              SimulateDataCapture(kFrames, kChannels);
                              SimulateDataCapture(kAltFrames, kAltChannels);
                            }),
                            /*expect_dropped_frames=*/true);
}

TEST_P(TrackAudioRendererTest, Integration_VariableChannels_RepeatedBuffers) {
  RunCaptureIntegrationTest(base::BindLambdaForTesting([this]() {
                              SimulateDataCapture(kFrames, kChannels);
                              SimulateDataCapture(kFrames, kChannels);
                              SimulateDataCapture(kAltFrames, kAltChannels);
                              SimulateDataCapture(kAltFrames, kAltChannels);
                            }),
                            /*expect_dropped_frames=*/true);
}

INSTANTIATE_TEST_SUITE_P(,
                         TrackAudioRendererTest,
                         testing::ValuesIn({true, false}));

}  // namespace blink

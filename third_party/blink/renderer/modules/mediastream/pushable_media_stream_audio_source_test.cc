// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"

#include "base/run_loop.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class FakeMediaStreamAudioSink : public WebMediaStreamAudioSink {
 public:
  FakeMediaStreamAudioSink(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> audio_thread)
      : main_task_runner_(std::move(main_thread)),
        audio_task_runner_(std::move(audio_thread)) {}
  ~FakeMediaStreamAudioSink() override = default;

  void SetupNewAudioParameterExpectations(int channels,
                                          int frames,
                                          int sample_rate) {
    expected_channels_ = channels;
    expected_frames_ = frames;
    expected_sample_rate_ = sample_rate;
  }

  void SetDataTimeExpectation(base::TimeTicks time, base::OnceClosure on_data) {
    DCHECK(!on_data_);

    expected_time_ = time;
    on_data_ = std::move(on_data);
  }

  void OnData(const media::AudioBus& data, base::TimeTicks time) override {
    // Make sure the source delivered audio data on the right thread.
    EXPECT_TRUE(audio_task_runner_->BelongsToCurrentThread());

    EXPECT_EQ(time, expected_time_);
    EXPECT_EQ(data.channels(), expected_channels_);
    EXPECT_EQ(data.frames(), expected_frames_);

    // Call this after all expectations are checked, to prevent test from
    // setting new expectations on the main thread.
    std::move(on_data_).Run();
  }

  void OnSetFormat(const media::AudioParameters& params) override {
    // Make sure the source changed parameters data on the right thread.
    EXPECT_TRUE(audio_task_runner_->BelongsToCurrentThread());

    // Also make sure that the audio thread is different from the main
    // thread (it would be a test error if it wasn't, as it would be
    // impossible for the check above to fail).
    ASSERT_TRUE(!main_task_runner_->BelongsToCurrentThread());

    // This should only be called once per format change.
    EXPECT_FALSE(did_receive_format_change_);

    EXPECT_EQ(params.sample_rate(), expected_sample_rate_);
    EXPECT_EQ(params.channels(), expected_channels_);
    EXPECT_EQ(params.frames_per_buffer(), expected_frames_);

    did_receive_format_change_ = true;
  }

  void ClearDidReceiveFormatChange() { did_receive_format_change_ = false; }

  bool did_receive_format_change() const { return did_receive_format_change_; }

 public:
  int expected_channels_ = 0;
  int expected_frames_ = 0;
  int expected_sample_rate_ = 0;
  base::TimeTicks expected_time_;

  bool did_receive_format_change_ = false;

  base::OnceClosure on_data_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;
};

}  // namespace

class PushableMediaStreamAudioSourceTest : public testing::Test {
 public:
  PushableMediaStreamAudioSourceTest() {
    // Use the IO thread for testing purposes. This is stricter than an audio
    // sequenced task runner needs to be.
    audio_task_runner_ = Platform::Current()->GetIOTaskRunner();
    main_task_runner_ = Thread::MainThread()->GetTaskRunner();

    pushable_audio_source_ = new PushableMediaStreamAudioSource(
        main_task_runner_, audio_task_runner_);
    stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeAudio, "dummy_source_name",
        false /* remote */);
    stream_source_->SetPlatformSource(base::WrapUnique(pushable_audio_source_));
    stream_component_ = MakeGarbageCollected<MediaStreamComponent>(
        stream_source_->Id(), stream_source_);
  }

  void TearDown() override {
    stream_source_ = nullptr;
    stream_component_ = nullptr;
    WebHeap::CollectAllGarbageForTesting();
  }

  bool ConnectSourceToTrack() {
    return pushable_audio_source_->ConnectToTrack(stream_component_);
  }

  void SendAndVerifyAudioData(FakeMediaStreamAudioSink* fake_sink,
                              int channels,
                              int frames,
                              int sample_rate,
                              bool expect_format_change) {
    fake_sink->ClearDidReceiveFormatChange();

    if (expect_format_change) {
      fake_sink->SetupNewAudioParameterExpectations(channels, frames,
                                                    sample_rate);
    }

    base::RunLoop run_loop;
    base::TimeTicks reference_time = base::TimeTicks::Now();
    fake_sink->SetDataTimeExpectation(reference_time, run_loop.QuitClosure());

    pushable_audio_source_->PushAudioData(AudioFrameSerializationData::Wrap(
        media::AudioBus::Create(channels, frames), sample_rate,
        reference_time - base::TimeTicks()));
    run_loop.Run();

    EXPECT_EQ(fake_sink->did_receive_format_change(), expect_format_change);
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  Persistent<MediaStreamComponent> stream_component_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_task_runner_;

  PushableMediaStreamAudioSource* pushable_audio_source_;
};

TEST_F(PushableMediaStreamAudioSourceTest, ConnectAndStop) {
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_audio_source_->running());

  EXPECT_TRUE(ConnectSourceToTrack());
  EXPECT_EQ(MediaStreamSource::kReadyStateLive,
            stream_source_->GetReadyState());
  EXPECT_TRUE(pushable_audio_source_->running());

  // If the pushable source stops, the MediaStreamSource should stop.
  pushable_audio_source_->StopSource();
  EXPECT_EQ(MediaStreamSource::kReadyStateEnded,
            stream_source_->GetReadyState());
  EXPECT_FALSE(pushable_audio_source_->running());
}

TEST_F(PushableMediaStreamAudioSourceTest, FramesPropagateToSink) {
  EXPECT_TRUE(ConnectSourceToTrack());
  FakeMediaStreamAudioSink fake_sink(main_task_runner_, audio_task_runner_);

  WebMediaStreamAudioSink::AddToAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));

  constexpr int kChannels = 1;
  constexpr int kFrames = 256;
  constexpr int kSampleRate = 8000;

  // The first audio data pushed should trigger a call to OnSetFormat().
  SendAndVerifyAudioData(&fake_sink, kChannels, kFrames, kSampleRate,
                         /*expect_format_change=*/true);

  // Using the same audio parameters should not trigger OnSetFormat().
  SendAndVerifyAudioData(&fake_sink, kChannels, kFrames, kSampleRate,
                         /*expect_format_change=*/false);

  // Format changes should trigger OnSetFormat().
  SendAndVerifyAudioData(&fake_sink, kChannels * 2, kFrames * 4,
                         /*sample_rate=*/44100, /*expect_format_change=*/true);

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &fake_sink, WebMediaStreamTrack(stream_component_.Get()));
}

}  // namespace blink

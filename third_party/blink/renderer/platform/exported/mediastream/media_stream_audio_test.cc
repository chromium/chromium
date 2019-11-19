// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/media_stream_audio_track.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_source.h"

namespace blink {

namespace {

constexpr int kSampleRate = 8000;
constexpr int kBufferSize = kSampleRate / 100;

// The maximum integer that can be exactly represented by the float data type.
constexpr int kMaxValueSafelyConvertableToFloat = 1 << 24;

// A simple MediaStreamAudioSource that spawns a real-time audio thread and
// emits audio samples with monotonically-increasing sample values. Includes
// hooks for the unit tests to confirm lifecycle status and to change audio
// format.
class FakeMediaStreamAudioSource : public MediaStreamAudioSource,
                                   public base::PlatformThread::Delegate {
 public:
  FakeMediaStreamAudioSource()
      : MediaStreamAudioSource(scheduler::GetSingleThreadTaskRunnerForTesting(),
                               true),
        stop_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                    base::WaitableEvent::InitialState::NOT_SIGNALED),
        next_buffer_size_(kBufferSize),
        sample_count_(0) {}

  ~FakeMediaStreamAudioSource() final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    EnsureSourceIsStopped();
  }

  bool was_started() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return !thread_.is_null();
  }

  bool was_stopped() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return stop_event_.IsSignaled();
  }

  void SetBufferSize(int new_buffer_size) {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    base::subtle::NoBarrier_Store(&next_buffer_size_, new_buffer_size);
  }

 protected:
  bool EnsureSourceIsStarted() final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    if (was_started())
      return true;
    if (was_stopped())
      return false;
    base::PlatformThread::CreateWithPriority(
        0, this, &thread_, base::ThreadPriority::REALTIME_AUDIO);
    return true;
  }

  void EnsureSourceIsStopped() final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    if (was_stopped())
      return;
    stop_event_.Signal();
    if (was_started())
      base::PlatformThread::Join(thread_);
  }

  void ThreadMain() override {
    while (!stop_event_.IsSignaled()) {
      // If needed, notify of the new format and re-create |audio_bus_|.
      const int buffer_size = base::subtle::NoBarrier_Load(&next_buffer_size_);
      if (!audio_bus_ || audio_bus_->frames() != buffer_size) {
        MediaStreamAudioSource::SetFormat(media::AudioParameters(
            media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
            media::CHANNEL_LAYOUT_MONO, kSampleRate, buffer_size));
        audio_bus_ = media::AudioBus::Create(1, buffer_size);
      }

      // Deliver the next chunk of audio data. Each sample value is its offset
      // from the very first sample.
      float* const data = audio_bus_->channel(0);
      for (int i = 0; i < buffer_size; ++i)
        data[i] = ++sample_count_;
      CHECK_LT(sample_count_, kMaxValueSafelyConvertableToFloat);
      MediaStreamAudioSource::DeliverDataToTracks(*audio_bus_,
                                                  base::TimeTicks::Now());

      // Sleep before producing the next chunk of audio.
      base::PlatformThread::Sleep(base::TimeDelta::FromMicroseconds(
          base::Time::kMicrosecondsPerSecond * buffer_size / kSampleRate));
    }
  }

 private:
  THREAD_CHECKER(main_thread_checker_);

  base::PlatformThreadHandle thread_;
  mutable base::WaitableEvent stop_event_;

  base::subtle::Atomic32 next_buffer_size_;
  std::unique_ptr<media::AudioBus> audio_bus_;
  int sample_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaStreamAudioSource);
};

// A simple WebMediaStreamAudioSink that consumes audio and confirms the
// sample values. Includes hooks for the unit tests to monitor the format and
// flow of audio, whether the audio is silent, and the propagation of the
// "enabled" state.
class FakeMediaStreamAudioSink : public WebMediaStreamAudioSink {
 public:
  enum EnableState { NO_ENABLE_NOTIFICATION, WAS_ENABLED, WAS_DISABLED };

  FakeMediaStreamAudioSink()
      : WebMediaStreamAudioSink(),
        expected_sample_count_(-1),
        num_on_data_calls_(0),
        audio_is_silent_(true),
        was_ended_(false),
        enable_state_(NO_ENABLE_NOTIFICATION) {}

  ~FakeMediaStreamAudioSink() final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
  }

  media::AudioParameters params() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    base::AutoLock auto_lock(params_lock_);
    return params_;
  }

  int num_on_data_calls() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return base::subtle::NoBarrier_Load(&num_on_data_calls_);
  }

  bool is_audio_silent() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return !!base::subtle::NoBarrier_Load(&audio_is_silent_);
  }

  bool was_ended() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return was_ended_;
  }

  EnableState enable_state() const {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    return enable_state_;
  }

  void OnSetFormat(const media::AudioParameters& params) final {
    ASSERT_TRUE(params.IsValid());
    base::AutoLock auto_lock(params_lock_);
    params_ = params;
  }

  void OnData(const media::AudioBus& audio_bus,
              base::TimeTicks estimated_capture_time) final {
    ASSERT_TRUE(params_.IsValid());
    ASSERT_FALSE(was_ended_);

    ASSERT_EQ(params_.channels(), audio_bus.channels());
    ASSERT_EQ(params_.frames_per_buffer(), audio_bus.frames());
    if (audio_bus.AreFramesZero()) {
      base::subtle::NoBarrier_Store(&audio_is_silent_, 1);
      expected_sample_count_ = -1;  // Reset for when audio comes back.
    } else {
      base::subtle::NoBarrier_Store(&audio_is_silent_, 0);
      const float* const data = audio_bus.channel(0);
      if (expected_sample_count_ == -1)
        expected_sample_count_ = static_cast<int64_t>(data[0]);
      CHECK_LE(expected_sample_count_ + audio_bus.frames(),
               kMaxValueSafelyConvertableToFloat);
      for (int i = 0; i < audio_bus.frames(); ++i) {
        const float expected_sample_value = expected_sample_count_;
        ASSERT_EQ(expected_sample_value, data[i]);
        ++expected_sample_count_;
      }
    }

    ASSERT_TRUE(!estimated_capture_time.is_null());
    ASSERT_LT(last_estimated_capture_time_, estimated_capture_time);
    last_estimated_capture_time_ = estimated_capture_time;

    base::subtle::NoBarrier_AtomicIncrement(&num_on_data_calls_, 1);
  }

  void OnReadyStateChanged(WebMediaStreamSource::ReadyState state) final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    if (state == WebMediaStreamSource::kReadyStateEnded)
      was_ended_ = true;
  }

  void OnEnabledChanged(bool enabled) final {
    DCHECK_CALLED_ON_VALID_THREAD(main_thread_checker_);
    enable_state_ = enabled ? WAS_ENABLED : WAS_DISABLED;
  }

 private:
  THREAD_CHECKER(main_thread_checker_);

  mutable base::Lock params_lock_;
  media::AudioParameters params_;
  int expected_sample_count_;
  base::TimeTicks last_estimated_capture_time_;
  base::subtle::Atomic32 num_on_data_calls_;
  base::subtle::Atomic32 audio_is_silent_;
  bool was_ended_;
  EnableState enable_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaStreamAudioSink);
};

}  // namespace

class MediaStreamAudioTest : public ::testing::Test {
 protected:
  void SetUp() override {
    blink_audio_source_.Initialize(
        WebString::FromUTF8("audio_id"), WebMediaStreamSource::kTypeAudio,
        WebString::FromUTF8("audio_track"), false /* remote */);
    blink_audio_track_.Initialize(blink_audio_source_.Id(),
                                  blink_audio_source_);
  }

  void TearDown() override {
    blink_audio_track_.Reset();
    blink_audio_source_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

  FakeMediaStreamAudioSource* source() const {
    return static_cast<FakeMediaStreamAudioSource*>(
        MediaStreamAudioSource::From(blink_audio_source_));
  }

  MediaStreamAudioTrack* track() const {
    return MediaStreamAudioTrack::From(blink_audio_track_);
  }

  WebMediaStreamSource blink_audio_source_;
  WebMediaStreamTrack blink_audio_track_;

  base::test::TaskEnvironment task_environment_;
};

// Tests that a simple source-->track-->sink connection and audio data flow
// works.
TEST_F(MediaStreamAudioTest, BasicUsage) {
  // Create the source, but it should not be started yet.
  ASSERT_FALSE(source());
  blink_audio_source_.SetPlatformSource(
      std::make_unique<FakeMediaStreamAudioSource>());
  ASSERT_TRUE(source());
  EXPECT_FALSE(source()->was_started());
  EXPECT_FALSE(source()->was_stopped());

  // Connect a track to the source. This should auto-start the source.
  ASSERT_FALSE(track());
  EXPECT_TRUE(source()->ConnectToTrack(blink_audio_track_));
  ASSERT_TRUE(track());
  EXPECT_TRUE(source()->was_started());
  EXPECT_FALSE(source()->was_stopped());

  // Connect a sink to the track. This should begin audio flow to the
  // sink. Wait and confirm that three OnData() calls were made from the audio
  // thread.
  FakeMediaStreamAudioSink sink;
  EXPECT_FALSE(sink.was_ended());
  track()->AddSink(&sink);
  const int start_count = sink.num_on_data_calls();
  while (sink.num_on_data_calls() - start_count < 3)
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // Check that the audio parameters propagated to the track and sink.
  const media::AudioParameters expected_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
      kSampleRate, kBufferSize);
  EXPECT_TRUE(expected_params.Equals(track()->GetOutputFormat()));
  EXPECT_TRUE(expected_params.Equals(sink.params()));

  // Stop the track. Since this was the last track connected to the source, the
  // source should automatically stop. In addition, the sink should receive a
  // ReadyStateEnded notification.
  track()->Stop();
  EXPECT_TRUE(source()->was_started());
  EXPECT_TRUE(source()->was_stopped());
  EXPECT_TRUE(sink.was_ended());

  track()->RemoveSink(&sink);
}

// Tests that "ended" tracks can be connected after the source has stopped.
TEST_F(MediaStreamAudioTest, ConnectTrackAfterSourceStopped) {
  // Create the source, connect one track, and stop it. This should
  // automatically stop the source.
  blink_audio_source_.SetPlatformSource(
      std::make_unique<FakeMediaStreamAudioSource>());
  ASSERT_TRUE(source());
  EXPECT_TRUE(source()->ConnectToTrack(blink_audio_track_));
  track()->Stop();
  EXPECT_TRUE(source()->was_started());
  EXPECT_TRUE(source()->was_stopped());

  // Now, connect another track. ConnectToTrack() will return false, but there
  // should be a MediaStreamAudioTrack instance created and owned by the
  // WebMediaStreamTrack.
  WebMediaStreamTrack another_blink_track;
  another_blink_track.Initialize(blink_audio_source_.Id(), blink_audio_source_);
  EXPECT_FALSE(MediaStreamAudioTrack::From(another_blink_track));
  EXPECT_FALSE(source()->ConnectToTrack(another_blink_track));
  EXPECT_TRUE(MediaStreamAudioTrack::From(another_blink_track));
}

// Tests that a sink is immediately "ended" when connected to a stopped track.
TEST_F(MediaStreamAudioTest, AddSinkToStoppedTrack) {
  // Create a track and stop it. Then, when adding a sink, the sink should get
  // the ReadyStateEnded notification immediately.
  MediaStreamAudioTrack track(true);
  track.Stop();
  FakeMediaStreamAudioSink sink;
  EXPECT_FALSE(sink.was_ended());
  track.AddSink(&sink);
  EXPECT_TRUE(sink.was_ended());
  EXPECT_EQ(0, sink.num_on_data_calls());
  track.RemoveSink(&sink);
}

// Tests that audio format changes at the source propagate to the track and
// sink.
TEST_F(MediaStreamAudioTest, FormatChangesPropagate) {
  // Create a source, connect it to track, and connect the track to a
  // sink.
  blink_audio_source_.SetPlatformSource(
      std::make_unique<FakeMediaStreamAudioSource>());
  ASSERT_TRUE(source());
  EXPECT_TRUE(source()->ConnectToTrack(blink_audio_track_));
  ASSERT_TRUE(track());
  FakeMediaStreamAudioSink sink;
  ASSERT_TRUE(!sink.params().IsValid());
  track()->AddSink(&sink);

  // Wait until valid parameters are propagated to the sink, and then confirm
  // the parameters are correct at the track and the sink.
  while (!sink.params().IsValid())
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  const media::AudioParameters expected_params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY, media::CHANNEL_LAYOUT_MONO,
      kSampleRate, kBufferSize);
  EXPECT_TRUE(expected_params.Equals(track()->GetOutputFormat()));
  EXPECT_TRUE(expected_params.Equals(sink.params()));

  // Now, trigger a format change by doubling the buffer size.
  source()->SetBufferSize(kBufferSize * 2);

  // Wait until the new buffer size propagates to the sink.
  while (sink.params().frames_per_buffer() == kBufferSize)
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_EQ(kBufferSize * 2, track()->GetOutputFormat().frames_per_buffer());
  EXPECT_EQ(kBufferSize * 2, sink.params().frames_per_buffer());

  track()->RemoveSink(&sink);
}

// Tests that tracks deliver audio when enabled and silent audio when
// disabled. Whenever a track is enabled or disabled, the sink's
// OnEnabledChanged() method should be called.
TEST_F(MediaStreamAudioTest, EnableAndDisableTracks) {
  // Create a source and connect it to track.
  blink_audio_source_.SetPlatformSource(
      std::make_unique<FakeMediaStreamAudioSource>());
  ASSERT_TRUE(source());
  EXPECT_TRUE(source()->ConnectToTrack(blink_audio_track_));
  ASSERT_TRUE(track());

  // Connect the track to a sink and expect the sink to be notified that the
  // track is enabled.
  FakeMediaStreamAudioSink sink;
  EXPECT_TRUE(sink.is_audio_silent());
  EXPECT_EQ(FakeMediaStreamAudioSink::NO_ENABLE_NOTIFICATION,
            sink.enable_state());
  track()->AddSink(&sink);
  EXPECT_EQ(FakeMediaStreamAudioSink::WAS_ENABLED, sink.enable_state());

  // Wait until non-silent audio reaches the sink.
  while (sink.is_audio_silent())
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // Now, disable the track and expect the sink to be notified.
  track()->SetEnabled(false);
  EXPECT_EQ(FakeMediaStreamAudioSink::WAS_DISABLED, sink.enable_state());

  // Wait until silent audio reaches the sink.
  while (!sink.is_audio_silent())
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // Create a second track and a second sink, but this time the track starts out
  // disabled. Expect the sink to be notified at the start that the track is
  // disabled.
  WebMediaStreamTrack another_blink_track;
  another_blink_track.Initialize(blink_audio_source_.Id(), blink_audio_source_);
  EXPECT_TRUE(source()->ConnectToTrack(another_blink_track));
  MediaStreamAudioTrack::From(another_blink_track)->SetEnabled(false);
  FakeMediaStreamAudioSink another_sink;
  MediaStreamAudioTrack::From(another_blink_track)->AddSink(&another_sink);
  EXPECT_EQ(FakeMediaStreamAudioSink::WAS_DISABLED,
            another_sink.enable_state());

  // Wait until OnData() is called on the second sink. Expect the audio to be
  // silent.
  const int start_count = another_sink.num_on_data_calls();
  while (another_sink.num_on_data_calls() == start_count)
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_TRUE(another_sink.is_audio_silent());

  // Now, enable the second track and expect the second sink to be notified.
  MediaStreamAudioTrack::From(another_blink_track)->SetEnabled(true);
  EXPECT_EQ(FakeMediaStreamAudioSink::WAS_ENABLED, another_sink.enable_state());

  // Wait until non-silent audio reaches the second sink.
  while (another_sink.is_audio_silent())
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  // The first track and sink should not have been affected by changing the
  // enabled state of the second track and sink. They should still be disabled,
  // with silent audio being consumed at the sink.
  EXPECT_EQ(FakeMediaStreamAudioSink::WAS_DISABLED, sink.enable_state());
  EXPECT_TRUE(sink.is_audio_silent());

  MediaStreamAudioTrack::From(another_blink_track)->RemoveSink(&another_sink);
  track()->RemoveSink(&sink);
}

}  // namespace blink

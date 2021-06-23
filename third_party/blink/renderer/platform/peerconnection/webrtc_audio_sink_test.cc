// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_audio_sink.h"

#include "media/base/fake_single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blink {

namespace {

class MockAudioSink : public webrtc::AudioTrackSinkInterface {
 public:
  MockAudioSink() = default;
  ~MockAudioSink() override = default;
  MOCK_METHOD6(OnData,
               void(const void* audio_data,
                    int bits_per_sample,
                    int sample_rate,
                    size_t number_of_channels,
                    size_t number_of_samples,
                    absl::optional<int64_t> absolute_capture_timestamp_ms));
};

class ScopedFakeClock : public rtc::ClockInterface {
 public:
  explicit ScopedFakeClock(int64_t init_time_ms)
      : prev_clock_(rtc::SetClockForTesting(this)),
        time_ns_(init_time_ms * rtc::kNumNanosecsPerMillisec) {}

  ~ScopedFakeClock() override { rtc::SetClockForTesting(prev_clock_); }

  int64_t TimeNanos() const override { return time_ns_; }

  void AdvanceTimeMilliseconds(int64_t time_ms) {
    time_ns_ += time_ms * rtc::kNumNanosecsPerMillisec;
  }

 private:
  ClockInterface* const prev_clock_;
  int64_t time_ns_;
};

}  // namespace

TEST(WebRtcAudioSinkTest, CaptureTimestamp) {
  MockAudioSink sink_1;
  MockAudioSink sink_2;
  base::SimpleTestTickClock dummy_clock;
  std::unique_ptr<WebRtcAudioSink> webrtc_audio_sink(
      new WebRtcAudioSink("test_sink", nullptr,
                          /*signaling_task_runner=*/
                          new media::FakeSingleThreadTaskRunner(&dummy_clock),
                          /*main_task_runner=*/
                          new media::FakeSingleThreadTaskRunner(&dummy_clock)));

  // |web_media_stream_audio_sink| is to access methods that are privately
  // inherited by WebRtcAudioSink.
  WebMediaStreamAudioSink* const web_media_stream_audio_sink =
      static_cast<WebMediaStreamAudioSink*>(webrtc_audio_sink.get());

  webrtc_audio_sink->webrtc_audio_track()->AddSink(&sink_1);
  webrtc_audio_sink->webrtc_audio_track()->AddSink(&sink_2);

  constexpr int kInputChannels = 2;
  constexpr int kInputFramesPerBuffer = 96;
  constexpr int kSampleRateHz = 8000;
  constexpr int kOutputFramesPerBuffer = kSampleRateHz / 100;
  constexpr int kEnqueueFrames = kInputFramesPerBuffer - kOutputFramesPerBuffer;

  constexpr int64_t kStartRtcTimestampMs = 87654321;
  constexpr int64_t kStartCaptureTimestampMs = 12345678;
  constexpr int64_t kCaptureIntervalMs = 567;

  web_media_stream_audio_sink->OnSetFormat(media::AudioParameters(
      media::AudioParameters::AUDIO_PCM_LINEAR, media::CHANNEL_LAYOUT_STEREO,
      kSampleRateHz, kOutputFramesPerBuffer));
  std::unique_ptr<media::AudioBus> bus =
      media::AudioBus::Create(kInputChannels, kInputFramesPerBuffer);
  bus->Zero();

  {
    ScopedFakeClock clock(kStartRtcTimestampMs);

    base::TimeTicks capture_time =
        base::TimeTicks() +
        base::TimeDelta::FromMilliseconds(kStartCaptureTimestampMs);

    // The first time to the call OnData(), the TimestampAligner should have no
    // effect work. So expected capture timestamp is from fake_clock.
    EXPECT_CALL(
        sink_1,
        OnData(_, _, kSampleRateHz, kInputChannels, kOutputFramesPerBuffer,
               absl::make_optional<int64_t>(kStartRtcTimestampMs)));
    EXPECT_CALL(
        sink_2,
        OnData(_, _, kSampleRateHz, kInputChannels, kOutputFramesPerBuffer,
               absl::make_optional<int64_t>(kStartRtcTimestampMs)));

    web_media_stream_audio_sink->OnData(*bus, capture_time);

    capture_time += base::TimeDelta::FromMilliseconds(kCaptureIntervalMs);
    clock.AdvanceTimeMilliseconds(kCaptureIntervalMs);

    constexpr int64_t kExpectedTimestampMs =
        kStartRtcTimestampMs + kCaptureIntervalMs -
        kEnqueueFrames * 1000 / kSampleRateHz;
    EXPECT_CALL(
        sink_1,
        OnData(_, _, kSampleRateHz, kInputChannels, kOutputFramesPerBuffer,
               absl::make_optional<int64_t>(kExpectedTimestampMs)));
    EXPECT_CALL(
        sink_2,
        OnData(_, _, kSampleRateHz, kInputChannels, kOutputFramesPerBuffer,
               absl::make_optional<int64_t>(kExpectedTimestampMs)));

    web_media_stream_audio_sink->OnData(*bus, capture_time);
  }
}

}  // namespace blink

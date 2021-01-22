// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/pushable_media_stream_audio_source.h"

#include "base/run_loop.h"
#include "media/base/bind_to_current_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-blink.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame_serialization_data.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

using testing::_;
using testing::WithArg;

namespace blink {

class PushableMediaStreamAudioSourceTest : public testing::Test {
 public:
  PushableMediaStreamAudioSourceTest() {
    // Use the IO thread for testing purposes. This is stricter than an audio
    // sequenced task runner needs to be.
    audio_thread_ = Platform::Current()->GetIOTaskRunner();
    main_thread_ = Thread::MainThread()->GetTaskRunner();

    pushable_audio_source_ =
        new PushableMediaStreamAudioSource(main_thread_, audio_thread_);
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

  void SendAndVerifyAudioData(MockMediaStreamAudioSink* mock_sink,
                              int channels,
                              int frames,
                              int sample_rate,
                              bool expect_format_change) {
    base::TimeTicks reference_time = base::TimeTicks::Now();

    base::RunLoop run_loop;
    if (expect_format_change) {
      EXPECT_CALL(*mock_sink, OnSetFormat(_))
          .WillOnce([&](const media::AudioParameters& params) {
            EXPECT_EQ(params.sample_rate(), sample_rate);
            EXPECT_EQ(params.channels(), channels);
            EXPECT_EQ(params.frames_per_buffer(), frames);
          });
    } else {
      EXPECT_CALL(*mock_sink, OnSetFormat(_)).Times(0);
    }

    EXPECT_CALL(*mock_sink, OnData(_, reference_time))
        .WillOnce(WithArg<0>([&](const media::AudioBus& data) {
          // Make sure we are on the right thread, and that the threads are
          // distinct.
          DCHECK(!main_thread_->BelongsToCurrentThread());
          DCHECK(audio_thread_->BelongsToCurrentThread());

          EXPECT_EQ(data.channels(), channels);
          EXPECT_EQ(data.frames(), frames);
          run_loop.Quit();
        }));

    pushable_audio_source_->PushAudioData(AudioFrameSerializationData::Wrap(
        media::AudioBus::Create(channels, frames), sample_rate,
        reference_time - base::TimeTicks()));
    run_loop.Run();
  }

 protected:
  ScopedTestingPlatformSupport<IOTaskRunnerTestingPlatformSupport> platform_;

  Persistent<MediaStreamSource> stream_source_;
  Persistent<MediaStreamComponent> stream_component_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_thread_;

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
  auto mock_sink =
      std::make_unique<::testing::StrictMock<MockMediaStreamAudioSink>>();

  WebMediaStreamAudioSink::AddToAudioTrack(
      mock_sink.get(), WebMediaStreamTrack(stream_component_.Get()));

  constexpr int kChannels = 1;
  constexpr int kFrames = 256;
  constexpr int kSampleRate = 8000;

  // The first audio data pushed should trigger a call to OnSetFormat().
  SendAndVerifyAudioData(mock_sink.get(), kChannels, kFrames, kSampleRate,
                         /*expect_format_change=*/true);

  // Using the same audio parameters should not trigger OnSetFormat().
  SendAndVerifyAudioData(mock_sink.get(), kChannels, kFrames, kSampleRate,
                         /*expect_format_change=*/false);

  // Format changes should trigger OnSetFormat().
  SendAndVerifyAudioData(mock_sink.get(), kChannels * 2, kFrames * 4,
                         /*sample_rate=*/44100, /*expect_format_change=*/true);

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      mock_sink.get(), WebMediaStreamTrack(stream_component_.Get()));
}

}  // namespace blink

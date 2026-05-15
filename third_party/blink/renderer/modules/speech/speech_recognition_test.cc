// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class SpeechRecognitionTest : public PageTestBase {
 public:
  void SetUp() override { PageTestBase::SetUp(); }
};

TEST_F(SpeechRecognitionTest, MediaStreamTrackAudioSinkCleanup) {
  ScopedMediaStreamTrackWebSpeechForTest media_stream_track_web_speech(true);

  V8TestingScope scope;
  LocalDOMWindow* window = &scope.GetWindow();

  auto* track = MakeGarbageCollected<MockMediaStreamTrack>();
  auto* source = MakeGarbageCollected<MediaStreamSource>(
      "id", MediaStreamSource::kTypeAudio, "name", false,
      /*platform_source=*/nullptr, MediaStreamSource::kReadyStateLive);
  auto* component = MakeGarbageCollected<MediaStreamComponentImpl>(
      source, std::make_unique<MediaStreamAudioTrack>(true));
  track->SetComponent(component);
  track->SetReadyState(MediaStreamSource::kReadyStateLive);

  auto* speech_recognition = SpeechRecognition::Create(window);

  EXPECT_CALL(*track, RegisterSink(testing::_)).Times(1);
  EXPECT_CALL(*track, UnregisterSink(testing::_)).Times(1);

  speech_recognition->start(track, scope.GetExceptionState());

  speech_recognition->abort();

  testing::Mock::VerifyAndClearExpectations(track);
  testing::Mock::AllowLeak(track);
}

}  // namespace blink

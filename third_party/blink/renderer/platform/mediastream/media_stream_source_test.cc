// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/mediastream/webaudio_destination_consumer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using ::testing::_;
using ::testing::StrictMock;

namespace blink {

class MockWebAudioDestinationConsumer : public WebAudioDestinationConsumer {
 public:
  MOCK_METHOD2(SetFormat, void(int, float));
  MOCK_METHOD2(ConsumeAudio, void(const Vector<const float*>&, int));
};

class MediaStreamSourceTest : public testing::Test {
 public:
  void SetUp() override {
    source = MakeGarbageCollected<MediaStreamSource>(
        String::FromUTF8("audio_id"), MediaStreamSource::kTypeAudio,
        String::FromUTF8("audio_track"), false /* remote */,
        MediaStreamSource::kReadyStateLive, true /* requires_consumer */);
    bus = AudioBus::Create(2, 10);
  }
  ~MediaStreamSourceTest() override = default;

 protected:
  StrictMock<MockWebAudioDestinationConsumer> consumer;
  Persistent<MediaStreamSource> source;
  scoped_refptr<AudioBus> bus;
};

TEST_F(MediaStreamSourceTest, AddAudioConsumer) {
  source->AddAudioConsumer(&consumer);

  EXPECT_CALL(consumer, ConsumeAudio(_, 10));

  source->ConsumeAudio(bus.get(), 10);
}

TEST_F(MediaStreamSourceTest, AddAudioConsumer_MultipleTimes) {
  // Add the same consumer multiple times.
  source->AddAudioConsumer(&consumer);
  source->AddAudioConsumer(&consumer);
  source->AddAudioConsumer(&consumer);

  // Should still only get one call.
  EXPECT_CALL(consumer, ConsumeAudio(_, 10)).Times(1);

  source->ConsumeAudio(bus.get(), 10);
}

TEST_F(MediaStreamSourceTest, RemoveAudioConsumer) {
  source->AddAudioConsumer(&consumer);
  EXPECT_TRUE(source->RemoveAudioConsumer(&consumer));

  // The consumer should get no calls.
  EXPECT_CALL(consumer, ConsumeAudio(_, 10)).Times(0);

  source->ConsumeAudio(bus.get(), 10);
}

}  // namespace blink

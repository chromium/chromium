// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"

#include <optional>

#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
        String::FromUTF8("audio_track"), /*remote=*/false,
        /*platform_source=*/nullptr, MediaStreamSource::kReadyStateLive,
        /*requires_consumer=*/true);
    bus = AudioBus::Create(2, 10);
  }
  ~MediaStreamSourceTest() override = default;

 protected:
  StrictMock<MockWebAudioDestinationConsumer> consumer;
  Persistent<MediaStreamSource> source;
  scoped_refptr<AudioBus> bus;
};

TEST_F(MediaStreamSourceTest, SetEmptyAudioConsumer) {
  source->SetAudioConsumer(nullptr);
}

TEST_F(MediaStreamSourceTest, SetAudioConsumer) {
  source->SetAudioConsumer(&consumer);

  EXPECT_CALL(consumer, ConsumeAudio(_, 10));

  source->ConsumeAudio(bus.get(), 10);
}

TEST_F(MediaStreamSourceTest, RemoveAudioConsumer) {
  source->SetAudioConsumer(&consumer);
  EXPECT_TRUE(source->RemoveAudioConsumer());

  // The consumer should get no calls.
  EXPECT_CALL(consumer, ConsumeAudio(_, 10)).Times(0);

  source->ConsumeAudio(bus.get(), 10);
}

TEST_F(MediaStreamSourceTest, ConsumeEmptyAudioConsumer) {
  // The consumer should get no calls.
  EXPECT_CALL(consumer, ConsumeAudio(_, 10)).Times(0);

  source->ConsumeAudio(bus.get(), 10);
}

TEST_F(MediaStreamSourceTest, RemoveEmptyAudioConsumer) {
  EXPECT_FALSE(source->RemoveAudioConsumer());
}
}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include "media/base/audio_encoder.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

using testing::_;
using testing::ByMove;
using testing::Return;
using testing::SaveArg;
using testing::WithArgs;

class MockAudioEncoder : public AudioEncoder {
 public:
  MockAudioEncoder(ScriptState* script_state,
                   const AudioEncoderInit* init,
                   ExceptionState& exception_state)
      : AudioEncoder(script_state, init, exception_state) {}
  ~MockAudioEncoder() override = default;

  MOCK_METHOD(std::unique_ptr<media::AudioEncoder>,
              CreateMediaAudioEncoder,
              (const ParsedConfig& config),
              (override));
};

class AudioEncoderTest : public testing::Test {
 public:
  AudioEncoderTest() = default;
  ~AudioEncoderTest() override = default;

  AudioData* CreateAudioData(ScriptState* script_state,
                             int channels,
                             int frames,
                             int sample_rate,
                             int timestamp_us) {
    auto* buffer = DOMArrayBuffer::Create(channels * frames, sizeof(float));
    auto* buffer_source = MakeGarbageCollected<AllowSharedBufferSource>(buffer);

    auto* audio_data_init = AudioDataInit::Create();
    audio_data_init->setData(buffer_source);
    audio_data_init->setTimestamp(timestamp_us);
    audio_data_init->setNumberOfChannels(channels);
    audio_data_init->setNumberOfFrames(frames);
    audio_data_init->setSampleRate(sample_rate);
    audio_data_init->setFormat(V8AudioSampleFormat::Enum::kF32Planar);

    DummyExceptionStateForTesting exception_state;
    return MakeGarbageCollected<AudioData>(script_state, audio_data_init,
                                           exception_state);
  }

  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(AudioEncoderTest, EncodeQueueSize) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function(script_state);
  auto* init = AudioEncoderInit::Create();
  init->setOutput(V8EncodedAudioChunkOutputCallback::Create(
      mock_function.ExpectNoCall()->ToV8Function(script_state)));
  init->setError(V8WebCodecsErrorCallback::Create(
      mock_function.ExpectNoCall()->ToV8Function(script_state)));

  auto* encoder =
      MakeGarbageCollected<MockAudioEncoder>(script_state, init, es);
  ASSERT_FALSE(es.HadException());

  auto* config = AudioEncoderConfig::Create();
  config->setCodec("opus");
  config->setSampleRate(48000);
  config->setNumberOfChannels(2);
  config->setBitrate(128000);

  auto media_encoder = std::make_unique<media::MockAudioEncoder>();
  media::MockAudioEncoder* mock_media_encoder = media_encoder.get();

  EXPECT_CALL(*encoder, CreateMediaAudioEncoder(_))
      .WillOnce(Return(ByMove(std::move(media_encoder))));

  EXPECT_CALL(*mock_media_encoder, Initialize(_, _, _))
      .WillOnce(WithArgs<2>([](media::AudioEncoder::EncoderStatusCB done_cb) {
        std::move(done_cb).Run(media::EncoderStatus::Codes::kOk);
      }));

  encoder->configure(config, es);
  ASSERT_FALSE(es.HadException());

  // Create AudioData to encode.
  auto* audio_data = CreateAudioData(script_state, 2, 480, 48000, 0);

  // Store callbacks to complete them manually.
  std::vector<media::AudioEncoder::EncoderStatusCB> encode_callbacks;

  EXPECT_CALL(*mock_media_encoder, Encode(_, _, _))
      .WillRepeatedly(WithArgs<2>(
          [&encode_callbacks](media::AudioEncoder::EncoderStatusCB done_cb) {
            encode_callbacks.push_back(std::move(done_cb));
          }));

  EXPECT_EQ(encoder->encodeQueueSize(), 0u);

  // Fill the queue.
  const size_t max_encodes =
      static_cast<size_t>(encoder->GetMaxActiveEncodesForTesting());
  const size_t extra_encodes = 2;
  for (size_t i = 0; i < max_encodes + extra_encodes; ++i) {
    encoder->encode(audio_data, es);
    ASSERT_FALSE(es.HadException());
  }

  // encode() will directly process the queue, so we'll never see more than the
  // number of encodes over `max_encodes` show up.
  EXPECT_EQ(encoder->encodeQueueSize(), extra_encodes);
  EXPECT_EQ(encode_callbacks.size(), max_encodes);

  // Now finish one encode.
  std::move(encode_callbacks[0]).Run(media::EncoderStatus::Codes::kOk);
  encode_callbacks.erase(encode_callbacks.begin());

  // One active slot opened up. One pending request should be moved to active.
  EXPECT_EQ(encoder->encodeQueueSize(), 1u);

  // We should have received another Encode call on media encoder.
  EXPECT_EQ(encode_callbacks.size(), max_encodes);

  // Finish remaining.
  for (auto& cb : encode_callbacks) {
    std::move(cb).Run(media::EncoderStatus::Codes::kOk);
  }
  encode_callbacks.clear();

  EXPECT_EQ(encoder->encodeQueueSize(), 0u);
  EXPECT_CALL(*mock_media_encoder, OnDestruct());
  encoder->close(es);
}

}  // namespace blink

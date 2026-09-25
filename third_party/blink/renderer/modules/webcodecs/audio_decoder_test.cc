// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include "media/base/audio_codecs.h"
#include "media/base/supported_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_support.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class AudioDecoderTest : public testing::Test {
 public:
  AudioDecoderTest() = default;
  ~AudioDecoderTest() override = default;

  AudioDecoderSupport* ToAudioDecoderSupport(V8TestingScope* v8_scope,
                                             ScriptValue value) {
    return NativeValueTraits<AudioDecoderSupport>::NativeValue(
        v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
  }

  test::TaskEnvironment task_environment_;
};

// Test ensures AudioCodecProfile parsing is happening correctly.
TEST_F(AudioDecoderTest, IsConfigSupported_XHE_AAC) {
  V8TestingScope scope;
  auto* config = AudioDecoderConfig::Create();
  config->setCodec("mp4a.40.42");
  config->setNumberOfChannels(2);
  config->setSampleRate(48000);

  ScriptPromise promise = AudioDecoder::isConfigSupported(
      scope.GetScriptState(), config, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* support = ToAudioDecoderSupport(&scope, tester.Value());
  ASSERT_TRUE(support);

  bool expected_support = media::IsDecoderSupportedAudioType(
      {media::AudioCodec::kAAC, media::AudioCodecProfile::kXHE_AAC});
  EXPECT_EQ(support->supported(), expected_support);
}

TEST_F(AudioDecoderTest, IsConfigSupported_Invalid) {
  V8TestingScope scope;
  auto* config = AudioDecoderConfig::Create();
  config->setCodec("invalid audio codec");
  config->setNumberOfChannels(2);
  config->setSampleRate(48000);

  ScriptPromise promise = AudioDecoder::isConfigSupported(
      scope.GetScriptState(), config, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* support = ToAudioDecoderSupport(&scope, tester.Value());
  ASSERT_TRUE(support);
  EXPECT_FALSE(support->supported());
}

TEST_F(AudioDecoderTest, IsConfigSupported_Opus_Non48k) {
  V8TestingScope scope;
  auto* config = AudioDecoderConfig::Create();
  config->setCodec("opus");
  config->setNumberOfChannels(2);
  config->setSampleRate(44100);

  ScriptPromise promise = AudioDecoder::isConfigSupported(
      scope.GetScriptState(), config, scope.GetExceptionState());
  ASSERT_FALSE(scope.GetExceptionState().HadException());

  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsFulfilled());

  auto* support = ToAudioDecoderSupport(&scope, tester.Value());
  ASSERT_TRUE(support);
  EXPECT_TRUE(support->supported());
}

TEST_F(AudioDecoderTest, IsConfigSupported_WithoutDescription) {
  V8TestingScope scope;

  struct TestCase {
    const char* codec;
    uint32_t channels;
  };
  constexpr uint32_t kSampleRate = 48000;
  constexpr TestCase kTestCases[] = {
      {"flac", 2},
      {"vorbis", 2},
      {"opus", 6},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.codec);
    auto* config = AudioDecoderConfig::Create();
    config->setCodec(test_case.codec);
    config->setNumberOfChannels(test_case.channels);
    config->setSampleRate(kSampleRate);

    ScriptPromise<AudioDecoderSupport> promise =
        AudioDecoder::isConfigSupported(scope.GetScriptState(), config,
                                        scope.GetExceptionState());
    ASSERT_FALSE(scope.GetExceptionState().HadException());

    ScriptPromiseTester tester(scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());

    auto* support = ToAudioDecoderSupport(&scope, tester.Value());
    ASSERT_TRUE(support);
    EXPECT_TRUE(support->supported());

    // `MakeMediaAudioDecoderConfig()` (used during `configure()`) should still
    // reject missing descriptions for these configurations.
    String js_error_message;
    EXPECT_FALSE(
        AudioDecoder::MakeMediaAudioDecoderConfig(*config, &js_error_message)
            .has_value());
    EXPECT_FALSE(js_error_message.empty());
  }
}

}  // namespace blink

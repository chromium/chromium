// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_pressure_manager_provider.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

template <class T>
class DecoderTemplateTest : public testing::Test {
 public:
  DecoderTemplateTest() = default;
  ~DecoderTemplateTest() override = default;

  typename T::ConfigType* CreateConfig();
  typename T::InitType* CreateInit(v8::Local<v8::Function> output_callback,
                                   v8::Local<v8::Function> error_callback);

  T* CreateDecoder(ScriptState*, const typename T::InitType*, ExceptionState&);
};

template <>
AudioDecoderConfig* DecoderTemplateTest<AudioDecoder>::CreateConfig() {
  auto* config = MakeGarbageCollected<AudioDecoderConfig>();
  config->setCodec("mp3");
  config->setSampleRate(48000);
  config->setNumberOfChannels(2);
  return config;
}

template <>
AudioDecoder* DecoderTemplateTest<AudioDecoder>::CreateDecoder(
    ScriptState* script_state,
    const AudioDecoderInit* init,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioDecoder>(script_state, init,
                                            exception_state);
}

template <>
AudioDecoderInit* DecoderTemplateTest<AudioDecoder>::CreateInit(
    v8::Local<v8::Function> output_callback,
    v8::Local<v8::Function> error_callback) {
  auto* init = MakeGarbageCollected<AudioDecoderInit>();
  init->setOutput(V8AudioDataOutputCallback::Create(output_callback));
  init->setError(V8WebCodecsErrorCallback::Create(error_callback));
  return init;
}

template <>
VideoDecoderConfig* DecoderTemplateTest<VideoDecoder>::CreateConfig() {
  auto* config = MakeGarbageCollected<VideoDecoderConfig>();
  config->setCodec("vp09.00.10.08");
  return config;
}

template <>
VideoDecoderInit* DecoderTemplateTest<VideoDecoder>::CreateInit(
    v8::Local<v8::Function> output_callback,
    v8::Local<v8::Function> error_callback) {
  auto* init = MakeGarbageCollected<VideoDecoderInit>();
  init->setOutput(V8VideoFrameOutputCallback::Create(output_callback));
  init->setError(V8WebCodecsErrorCallback::Create(error_callback));
  return init;
}

template <>
VideoDecoder* DecoderTemplateTest<VideoDecoder>::CreateDecoder(
    ScriptState* script_state,
    const VideoDecoderInit* init,
    ExceptionState& exception_state) {
  return VideoDecoder::Create(script_state, init, exception_state);
}

using DecoderTemplateImplementations =
    testing::Types<AudioDecoder, VideoDecoder>;

TYPED_TEST_SUITE(DecoderTemplateTest, DecoderTemplateImplementations);

TYPED_TEST(DecoderTemplateTest, BasicConstruction) {
  V8TestingScope v8_scope;

  MockFunctionScope mock_function(v8_scope.GetScriptState());
  auto* decoder =
      this->CreateDecoder(v8_scope.GetScriptState(),
                          this->CreateInit(mock_function.ExpectNoCall(),
                                           mock_function.ExpectNoCall()),
                          v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  EXPECT_FALSE(v8_scope.GetExceptionState().HadException());
}

TYPED_TEST(DecoderTemplateTest, ResetDuringFlush) {
  V8TestingScope v8_scope;

  // Create a decoder.
  MockFunctionScope mock_function(v8_scope.GetScriptState());
  auto* decoder =
      this->CreateDecoder(v8_scope.GetScriptState(),
                          this->CreateInit(mock_function.ExpectNoCall(),
                                           mock_function.ExpectNoCall()),
                          v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  // Configure the decoder.
  decoder->configure(this->CreateConfig(), v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  // flush() to ensure configure completes.
  {
    auto promise = decoder->flush(v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  // flush() again but reset() before it gets started.
  {
    auto promise = decoder->flush(v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
    decoder->reset(v8_scope.GetExceptionState());
    ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsRejected());
  }
}

// Ensures codecs do not apply reclamation pressure by default.
// Sheriff 2022/02/25; flaky test crbug/1300845
TYPED_TEST(DecoderTemplateTest, DISABLED_NoPressureByDefault) {
  V8TestingScope v8_scope;

  // Create a decoder.
  MockFunctionScope mock_function(v8_scope.GetScriptState());
  auto* decoder =
      this->CreateDecoder(v8_scope.GetScriptState(),
                          this->CreateInit(mock_function.ExpectNoCall(),
                                           mock_function.ExpectNoCall()),
                          v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  // Configure the decoder.
  decoder->configure(this->CreateConfig(), v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  // Codecs shouldn't apply pressure by default.
  ASSERT_FALSE(decoder->is_applying_codec_pressure());

  auto* decoder_pressure_manager =
      CodecPressureManagerProvider::From(*v8_scope.GetExecutionContext())
          .GetDecoderPressureManager();

  ASSERT_EQ(0u, decoder_pressure_manager->pressure_for_testing());
}

}  // namespace

}  // namespace blink

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/core/testing/mock_function_scope.h"

namespace blink {

namespace {

class AudioEncoderTest : public testing::Test {
 public:
  AudioEncoderTest() = default;
  ~AudioEncoderTest() override = default;
};

AudioEncoderConfig* CreateConfig() {
  auto* config = MakeGarbageCollected<AudioEncoderConfig>();
  config->setCodec("opus");
  config->setSampleRate(44100);
  config->setNumberOfChannels(2);
  return config;
}

AudioEncoder* CreateEncoder(ScriptState* script_state,
                            const AudioEncoderInit* init,
                            ExceptionState& exception_state) {
  return MakeGarbageCollected<AudioEncoder>(script_state, init,
                                            exception_state);
}

AudioEncoderInit* CreateInit(v8::Local<v8::Function> output_callback,
                             v8::Local<v8::Function> error_callback) {
  auto* init = MakeGarbageCollected<AudioEncoderInit>();
  init->setOutput(V8EncodedAudioChunkOutputCallback::Create(output_callback));
  init->setError(V8WebCodecsErrorCallback::Create(error_callback));
  return init;
}

TEST_F(AudioEncoderTest, CodecReclamation) {
  V8TestingScope v8_scope;
  auto& es = v8_scope.GetExceptionState();
  auto* script_state = v8_scope.GetScriptState();

  MockFunctionScope mock_function_scope(script_state);

  // Create an audio encoder.
  auto* init = CreateInit(mock_function_scope.ExpectNoCall(),
                          mock_function_scope.ExpectCall());
  auto* encoder = CreateEncoder(script_state, init, es);
  ASSERT_FALSE(es.HadException());

  // Simulate backgrounding to enable reclamation.
  if (!encoder->is_backgrounded_for_testing()) {
    encoder->SimulateLifecycleStateForTesting(
        scheduler::SchedulingLifecycleState::kHidden);
    DCHECK(encoder->is_backgrounded_for_testing());
  }

  auto* config = CreateConfig();
  encoder->configure(config, es);
  ASSERT_FALSE(es.HadException());
  {
    // We need this to make sure that configuration has completed.
    auto promise = encoder->flush(es);
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  // The encoder should be active, for reclamation purposes.
  ASSERT_TRUE(encoder->IsReclamationTimerActiveForTesting());

  // Resetting the encoder should prevent codec reclamation, silently.
  encoder->reset(es);
  ASSERT_FALSE(encoder->IsReclamationTimerActiveForTesting());

  // Reconfiguring the encoder should restart the reclamation timer.
  encoder->configure(config, es);
  ASSERT_FALSE(es.HadException());
  {
    // We need this to make sure that configuration has completed.
    auto promise = encoder->flush(es);
    ScriptPromiseTester tester(script_state, promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  ASSERT_TRUE(encoder->IsReclamationTimerActiveForTesting());

  // Reclaiming a configured encoder should call the error callback.
  encoder->SimulateCodecReclaimedForTesting();
  ASSERT_FALSE(encoder->IsReclamationTimerActiveForTesting());
}

}  // namespace

}  // namespace blink

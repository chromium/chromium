// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_underlying_source_wrapper.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/unguessable_token.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/test/mock_transformable_audio_frame.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"

namespace blink {

using ::testing::NiceMock;
using webrtc::MockTransformableAudioFrame;
using webrtc::MockTransformableVideoFrame;

class RTCEncodedUnderlyingSourceWrapperTest : public testing::Test {
 public:
  RTCEncodedUnderlyingSourceWrapper* CreateSource(ScriptState* script_state) {
    return MakeGarbageCollected<RTCEncodedUnderlyingSourceWrapper>(
        script_state);
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::MockOnceClosure disconnect_callback_;
};

TEST_F(RTCEncodedUnderlyingSourceWrapperTest,
       AudioSourceDataFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  source->CreateAudioUnderlyingSource(
      WTF::CrossThreadBindOnce(disconnect_callback_.Get()),
      base::UnguessableToken::Create());
  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  source->GetAudioTransformer().Run(
      std::make_unique<NiceMock<MockTransformableAudioFrame>>());
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  EXPECT_TRUE(result->IsObject());
  v8::Local<v8::Value> v8_signal;
  bool done = false;
  EXPECT_TRUE(V8UnpackIterationResult(script_state, result.As<v8::Object>(),
                                      &v8_signal, &done));
  EXPECT_FALSE(done);
  auto* rtc_encoded_audio_frame =
      NativeValueTraits<RTCEncodedAudioFrame>::NativeValue(
          v8_scope.GetIsolate(), v8_signal, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(rtc_encoded_audio_frame);

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedUnderlyingSourceWrapperTest, AudioCancelStream) {
  V8TestingScope v8_scope;
  auto* source = CreateSource(v8_scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);
  source->CreateAudioUnderlyingSource(
      WTF::CrossThreadBindOnce(disconnect_callback_.Get()),
      base::UnguessableToken::Create());
  EXPECT_CALL(disconnect_callback_, Run());
  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);
}

TEST_F(RTCEncodedUnderlyingSourceWrapperTest,
       VideoSourceDataFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);
  source->CreateVideoUnderlyingSource(
      WTF::CrossThreadBindOnce(disconnect_callback_.Get()),
      base::UnguessableToken::Create());
  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  source->GetVideoTransformer().Run(
      std::make_unique<NiceMock<MockTransformableVideoFrame>>());
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  v8::Local<v8::Value> result = read_tester.Value().V8Value();
  EXPECT_TRUE(result->IsObject());
  v8::Local<v8::Value> v8_signal;
  bool done = false;
  EXPECT_TRUE(V8UnpackIterationResult(script_state, result.As<v8::Object>(),
                                      &v8_signal, &done));
  EXPECT_FALSE(done);
  auto* rtc_encoded_video_frame =
      NativeValueTraits<RTCEncodedVideoFrame>::NativeValue(
          v8_scope.GetIsolate(), v8_signal, ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(rtc_encoded_video_frame);
  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedUnderlyingSourceWrapperTest, VideoCancelStream) {
  V8TestingScope v8_scope;
  auto* source = CreateSource(v8_scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);
  source->CreateVideoUnderlyingSource(
      WTF::CrossThreadBindOnce(disconnect_callback_.Get()),
      base::UnguessableToken::Create());
  EXPECT_CALL(disconnect_callback_, Run());
  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);
}
}  // namespace blink

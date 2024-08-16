// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_underlying_sink_wrapper.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/api/test/mock_transformable_audio_frame.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using webrtc::MockTransformableVideoFrame;

namespace blink {

const uint32_t kSSRC = 1;

namespace {

class MockWebRtcTransformedFrameCallback
    : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD1(OnTransformedFrame,
               void(std::unique_ptr<webrtc::TransformableFrameInterface>));
};

}  // namespace

class RTCEncodedUnderlyingSinkWrapperTest : public testing::Test {
 public:
  RTCEncodedUnderlyingSinkWrapperTest()
      : main_task_runner_(
            blink::scheduler::GetSingleThreadTaskRunnerForTesting()),
        webrtc_callback_(
            new rtc::RefCountedObject<MockWebRtcTransformedFrameCallback>()),
        audio_transformer_(main_task_runner_),
        video_transformer_(main_task_runner_, /*metronome*/ nullptr) {}

  void SetUp() override {
    EXPECT_FALSE(audio_transformer_.HasTransformedFrameCallback());
    audio_transformer_.RegisterTransformedFrameCallback(webrtc_callback_);
    EXPECT_TRUE(audio_transformer_.HasTransformedFrameCallback());
    EXPECT_FALSE(video_transformer_.HasTransformedFrameSinkCallback(kSSRC));
    video_transformer_.RegisterTransformedFrameSinkCallback(webrtc_callback_,
                                                            kSSRC);
    EXPECT_TRUE(video_transformer_.HasTransformedFrameSinkCallback(kSSRC));
  }

  void TearDown() override {
    platform_->RunUntilIdle();
    audio_transformer_.UnregisterTransformedFrameCallback();
    EXPECT_FALSE(audio_transformer_.HasTransformedFrameCallback());
    video_transformer_.UnregisterTransformedFrameSinkCallback(kSSRC);
    EXPECT_FALSE(video_transformer_.HasTransformedFrameSinkCallback(kSSRC));
  }

  RTCEncodedUnderlyingSinkWrapper* CreateSink(ScriptState* script_state) {
    return MakeGarbageCollected<RTCEncodedUnderlyingSinkWrapper>(script_state);
  }

  RTCEncodedAudioStreamTransformer* GetAudioTransformer() {
    return &audio_transformer_;
  }
  RTCEncodedVideoStreamTransformer* GetVideoTransformer() {
    return &video_transformer_;
  }

  RTCEncodedAudioFrame* CreateEncodedAudioFrame(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction direction =
          webrtc::TransformableFrameInterface::Direction::kSender,
      size_t payload_length = 100,
      bool expect_data_read = false) {
    auto mock_frame =
        std::make_unique<NiceMock<webrtc::MockTransformableAudioFrame>>();
    ON_CALL(*mock_frame.get(), GetDirection).WillByDefault(Return(direction));
    if (expect_data_read) {
      EXPECT_CALL(*mock_frame.get(), GetData)
          .WillOnce(
              Return(rtc::ArrayView<const uint8_t>(buffer, payload_length)));
    } else {
      EXPECT_CALL(*mock_frame.get(), GetData).Times(0);
    }
    std::unique_ptr<webrtc::TransformableAudioFrameInterface> audio_frame =
        base::WrapUnique(static_cast<webrtc::TransformableAudioFrameInterface*>(
            mock_frame.release()));
    return MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(audio_frame));
  }

  ScriptValue CreateEncodedAudioFrameChunk(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction direction =
          webrtc::TransformableFrameInterface::Direction::kSender) {
    return ScriptValue(
        script_state->GetIsolate(),
        ToV8Traits<RTCEncodedAudioFrame>::ToV8(
            script_state, CreateEncodedAudioFrame(script_state, direction)));
  }

  ScriptValue CreateEncodedVideoFrameChunk(
      ScriptState* script_state,
      webrtc::TransformableFrameInterface::Direction direction =
          webrtc::TransformableFrameInterface::Direction::kSender) {
    auto mock_frame = std::make_unique<NiceMock<MockTransformableVideoFrame>>();

    ON_CALL(*mock_frame.get(), GetSsrc).WillByDefault(Return(kSSRC));
    ON_CALL(*mock_frame.get(), GetDirection).WillByDefault(Return(direction));
    RTCEncodedVideoFrame* frame =
        MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(mock_frame));
    return ScriptValue(
        script_state->GetIsolate(),
        ToV8Traits<RTCEncodedVideoFrame>::ToV8(script_state, frame));
  }

 protected:
  test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  rtc::scoped_refptr<MockWebRtcTransformedFrameCallback> webrtc_callback_;
  RTCEncodedAudioStreamTransformer audio_transformer_;
  RTCEncodedVideoStreamTransformer video_transformer_;
  uint8_t buffer[1500];
};

TEST_F(RTCEncodedUnderlyingSinkWrapperTest,
       WriteToStreamForwardsToWebRtcCallbackAudio) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateAudioUnderlyingSink(audio_transformer_.GetBroker());
  auto* stream =
      WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1u);

  NonThrowableExceptionState exception_state;
  auto* writer = stream->getWriter(script_state, exception_state);

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateEncodedAudioFrameChunk(script_state),
                    exception_state));
  EXPECT_FALSE(write_tester.IsFulfilled());

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, CreateEncodedAudioFrameChunk(script_state),
              /*controller=*/nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, WriteInvalidDataFailsAudio) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateAudioUnderlyingSink(audio_transformer_.GetBroker());
  ScriptValue v8_integer =
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 0));

  // Writing something that is not an RTCEncodedAudioFrame integer to the sink
  // should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, v8_integer, /*controller=*/nullptr,
              dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest,
       WriteInDifferentDirectionIsAllowedAudio) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateAudioUnderlyingSink(audio_transformer_.GetBroker());
  // Write an encoded chunk with direction set to Receiver should work even
  // though it doesn't match the direction of sink creation.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state,
              CreateEncodedAudioFrameChunk(
                  script_state,
                  webrtc::TransformableFrameInterface::Direction::kReceiver),
              /*controller=*/nullptr, dummy_exception_state);
  EXPECT_FALSE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest,
       WriteToStreamForwardsToWebRtcCallbackVideo) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateVideoUnderlyingSink(video_transformer_.GetBroker());
  auto* stream =
      WritableStream::CreateWithCountQueueingStrategy(script_state, sink, 1u);

  NonThrowableExceptionState exception_state;
  auto* writer = stream->getWriter(script_state, exception_state);

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, CreateEncodedVideoFrameChunk(script_state),
                    exception_state));
  EXPECT_FALSE(write_tester.IsFulfilled());

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, CreateEncodedVideoFrameChunk(script_state), nullptr,
              dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, WriteInvalidDataFailsVideo) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateVideoUnderlyingSink(video_transformer_.GetBroker());
  ScriptValue v8_integer =
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 0));

  // Writing something that is not an RTCEncodedVideoFrame integer to the sink
  // should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state, v8_integer, nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, WritingSendFrameSucceedsVideo) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateVideoUnderlyingSink(video_transformer_.GetBroker());

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));

  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state,
              CreateEncodedVideoFrameChunk(
                  script_state,
                  webrtc::TransformableFrameInterface::Direction::kSender),
              nullptr, dummy_exception_state);
  EXPECT_FALSE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, WritingReceiverFrameSucceedsVideo) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);
  sink->CreateVideoUnderlyingSink(video_transformer_.GetBroker());

  EXPECT_CALL(*webrtc_callback_, OnTransformedFrame(_));

  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state,
              CreateEncodedVideoFrameChunk(
                  script_state,
                  webrtc::TransformableFrameInterface::Direction::kReceiver),
              nullptr, dummy_exception_state);
  EXPECT_FALSE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, WritingBeforeAudioOrVideoIsSetup) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);

  DummyExceptionStateForTesting dummy_exception_state;
  sink->write(script_state,
              CreateEncodedVideoFrameChunk(
                  script_state,
                  webrtc::TransformableFrameInterface::Direction::kReceiver),
              nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, ClosingBeforeAudioOrVideoIsSetup) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);

  DummyExceptionStateForTesting dummy_exception_state;
  sink->close(script_state, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

TEST_F(RTCEncodedUnderlyingSinkWrapperTest, AbortingBeforeAudioOrVideoIsSetup) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateSink(script_state);

  DummyExceptionStateForTesting dummy_exception_state;
  sink->abort(script_state, ScriptValue(), dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
}

}  // namespace blink

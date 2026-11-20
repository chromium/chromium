// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_receiver_source_optimizer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_sender_source_optimizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"

namespace blink {

using webrtc::MockTransformableVideoFrame;

class RTCEncodedVideoUnderlyingSourceTest : public testing::Test {
 public:
  RTCEncodedVideoUnderlyingSource* CreateSource(ScriptState* script_state) {
    return MakeGarbageCollected<RTCEncodedVideoUnderlyingSource>(
        script_state, CrossThreadBindOnce(disconnect_callback_.Get()));
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::MockOnceClosure disconnect_callback_;
};

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       SourceDataFlowsThroughStreamAndCloses) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  NonThrowableExceptionState exception_state;
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, exception_state);

  ScriptPromiseTester read_tester(script_state,
                                  reader->read(script_state, exception_state));
  EXPECT_FALSE(read_tester.IsFulfilled());
  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  read_tester.WaitUntilSettled();
  EXPECT_TRUE(read_tester.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, CancelStream) {
  V8TestingScope v8_scope;
  auto* source = CreateSource(v8_scope.GetScriptState());
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      v8_scope.GetScriptState(), source, 0);

  EXPECT_CALL(disconnect_callback_, Run());
  NonThrowableExceptionState exception_state;
  stream->cancel(v8_scope.GetScriptState(), exception_state);
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, QueuedFramesAreDroppedWhenOverflow) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  // Create a stream, to ensure there is a controller associated to the source.
  ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                  source, 0);
  for (int i = 0; i > RTCEncodedVideoUnderlyingSource::kMinQueueDesiredSize;
       --i) {
    EXPECT_EQ(source->Controller()->DesiredSize(), i);
    source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  }
  EXPECT_EQ(source->Controller()->DesiredSize(),
            RTCEncodedVideoUnderlyingSource::kMinQueueDesiredSize);

  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  EXPECT_EQ(source->Controller()->DesiredSize(),
            RTCEncodedVideoUnderlyingSource::kMinQueueDesiredSize);

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, AvoidsLeaseOnMainThread) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcUseMediaThreadTypes);
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  EXPECT_FALSE(source->GetRealmThreadTypeLeasedForTesting().has_value());
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       AvoidsLeasesOnMainThreadWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebRtcUseMediaThreadTypes);
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  EXPECT_FALSE(source->GetRealmThreadTypeLeasedForTesting().has_value());
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, LeasesOnWorker) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcUseMediaThreadTypes);
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  source->SetRealmIsBoostableContextForTesting(true);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  ASSERT_EQ(source->GetRealmThreadTypeLeasedForTesting(),
            base::ThreadType::kPresentation);
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       AvoidsLeasesOnWorkerWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kWebRtcUseMediaThreadTypes);
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  source->SetRealmIsBoostableContextForTesting(true);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
  EXPECT_FALSE(source->GetRealmThreadTypeLeasedForTesting().has_value());
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       ContextDestroyedCallsDisconnectCallback) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kWebRtcUseMediaThreadTypes);
  Persistent<RTCEncodedVideoUnderlyingSource> source;
  {
    V8TestingScope v8_scope;
    ScriptState* script_state = v8_scope.GetScriptState();
    source = CreateSource(script_state);
    source->SetRealmIsBoostableContextForTesting(true);
    ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

    source->OnFrameFromSource(std::make_unique<MockTransformableVideoFrame>());
    ASSERT_EQ(source->GetRealmThreadTypeLeasedForTesting(),
              base::ThreadType::kPresentation);

    EXPECT_CALL(disconnect_callback_, Run());
  }
  EXPECT_FALSE(source->GetRealmThreadTypeLeasedForTesting().has_value());
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       TransferResetsDisconnectCallbackBeforeContextDestroyed) {
  EXPECT_CALL(disconnect_callback_, Run()).Times(0);
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  source->OnSourceTransferStarted();
  task_environment_.RunUntilIdle();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       ReceiverOptimizerAfterContextDestroyedDisconnectsAndReturnsNull) {
  V8TestingScope v8_scope;
  base::MockRepeatingCallback<void(RTCEncodedVideoUnderlyingSource*,
                                   scoped_refptr<base::SingleThreadTaskRunner>)>
      set_underlying_source;
  RtcEncodedVideoReceiverSourceOptimizer optimizer(
      CrossThreadBindRepeating(set_underlying_source.Get()),
      CrossThreadBindOnce(disconnect_callback_.Get()));

  v8_scope.GetExecutionContext()->NotifyContextDestroyed();

  EXPECT_CALL(set_underlying_source, Run(testing::_, testing::_)).Times(0);
  EXPECT_CALL(disconnect_callback_, Run()).Times(1);
  EXPECT_EQ(optimizer.PerformInProcessOptimization(v8_scope.GetScriptState()),
            nullptr);
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       SenderOptimizerAfterContextDestroyedDisconnectsAndReturnsNull) {
  V8TestingScope v8_scope;
  base::MockRepeatingCallback<void(RTCEncodedVideoUnderlyingSource*,
                                   scoped_refptr<base::SingleThreadTaskRunner>)>
      set_underlying_source;
  RtcEncodedVideoSenderSourceOptimizer optimizer(
      CrossThreadBindRepeating(set_underlying_source.Get()),
      CrossThreadBindOnce(disconnect_callback_.Get()));

  v8_scope.GetExecutionContext()->NotifyContextDestroyed();

  EXPECT_CALL(set_underlying_source, Run(testing::_, testing::_)).Times(0);
  EXPECT_CALL(disconnect_callback_, Run()).Times(1);
  EXPECT_EQ(optimizer.PerformInProcessOptimization(v8_scope.GetScriptState()),
            nullptr);
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       PendingKeyFrameRequestResolvedOnKeyFrame) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester promise_tester(script_state, resolver->Promise());
  source->AddPendingKeyFrameRequest({g_empty_string}, resolver);

  // Delta frame should not resolve the pending keyframe request.
  auto delta_frame =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*delta_frame, IsKeyFrame()).WillByDefault(testing::Return(false));
  source->OnFrameFromSource(std::move(delta_frame));
  v8_scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(promise_tester.IsFulfilled());

  // Keyframe should resolve the pending request.
  auto key_frame =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*key_frame, IsKeyFrame()).WillByDefault(testing::Return(true));
  source->OnFrameFromSource(std::move(key_frame));
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, PendingKeyFrameRequestMatchesRid) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester promise_tester(script_state, resolver->Promise());
  source->AddPendingKeyFrameRequest({"1"}, resolver);

  // Keyframe for rid "0" should not resolve the request for rid "1".
  auto frame_rid0 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid0, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid0, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("0")));
  source->OnFrameFromSource(std::move(frame_rid0));
  v8_scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(promise_tester.IsFulfilled());

  // Keyframe for rid "1" should resolve the request.
  auto frame_rid1 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid1, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid1, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("1")));
  source->OnFrameFromSource(std::move(frame_rid1));
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       PendingKeyFrameRequestWaitsForAllRids) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester promise_tester(script_state, resolver->Promise());
  source->AddPendingKeyFrameRequest({"0", "1"}, resolver);

  // Keyframe for rid "0" removes "0" but leaves the request pending for "1".
  auto frame_rid0 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid0, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid0, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("0")));
  source->OnFrameFromSource(std::move(frame_rid0));
  v8_scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(promise_tester.IsFulfilled());

  // Delta frame for rid "1" does not resolve the request.
  auto delta_rid1 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*delta_rid1, IsKeyFrame()).WillByDefault(testing::Return(false));
  ON_CALL(*delta_rid1, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("1")));
  source->OnFrameFromSource(std::move(delta_rid1));
  v8_scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(promise_tester.IsFulfilled());

  // Keyframe for rid "1" completes the set and resolves the request.
  auto frame_rid1 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid1, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid1, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("1")));
  source->OnFrameFromSource(std::move(frame_rid1));
  promise_tester.WaitUntilSettled();
  EXPECT_TRUE(promise_tester.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       OverlappingPendingKeyFrameRequestsResolveIndependently) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester tester1(script_state, resolver1->Promise());
  source->AddPendingKeyFrameRequest({"0"}, resolver1);

  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester tester2(script_state, resolver2->Promise());
  source->AddPendingKeyFrameRequest({"0", "1"}, resolver2);

  // Keyframe for rid "0" resolves the {"0"} request, while {"0", "1"} remains
  // pending waiting for "1".
  auto frame_rid0 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid0, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid0, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("0")));
  source->OnFrameFromSource(std::move(frame_rid0));
  tester1.WaitUntilSettled();
  EXPECT_TRUE(tester1.IsFulfilled());
  EXPECT_FALSE(tester2.IsFulfilled());

  // Keyframe for rid "1" resolves the {"0", "1"} request.
  auto frame_rid1 =
      std::make_unique<testing::NiceMock<MockTransformableVideoFrame>>();
  ON_CALL(*frame_rid1, IsKeyFrame()).WillByDefault(testing::Return(true));
  ON_CALL(*frame_rid1, Rid())
      .WillByDefault(testing::Return(std::optional<std::string>("1")));
  source->OnFrameFromSource(std::move(frame_rid1));
  tester2.WaitUntilSettled();
  EXPECT_TRUE(tester2.IsFulfilled());

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest,
       PendingKeyFrameRequestDetachedOnCancelAndAfterClose) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  auto* stream =
      ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver1 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester tester1(script_state, resolver1->Promise());
  source->AddPendingKeyFrameRequest({g_empty_string}, resolver1);

  EXPECT_CALL(disconnect_callback_, Run());
  stream->cancel(script_state, v8_scope.GetExceptionState());
  v8_scope.PerformMicrotaskCheckpoint();

  // Adding a request after Cancel() detaches immediately.
  auto* resolver2 =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  ScriptPromiseTester tester2(script_state, resolver2->Promise());
  source->AddPendingKeyFrameRequest({g_empty_string}, resolver2);

  resolver1 = nullptr;
  resolver2 = nullptr;
  ThreadState::Current()->CollectAllGarbageForTesting();
  v8_scope.PerformMicrotaskCheckpoint();
  EXPECT_FALSE(tester1.IsFulfilled());
  EXPECT_FALSE(tester1.IsRejected());
  EXPECT_FALSE(tester2.IsFulfilled());
  EXPECT_FALSE(tester2.IsRejected());
}

TEST_F(RTCEncodedVideoUnderlyingSourceTest, RemovePendingKeyFrameRequest) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* source = CreateSource(script_state);
  ReadableStream::CreateWithCountQueueingStrategy(script_state, source, 0);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  source->AddPendingKeyFrameRequest({"0"}, resolver);
  EXPECT_TRUE(source->RemovePendingKeyFrameRequest(resolver));
  EXPECT_FALSE(source->RemovePendingKeyFrameRequest(resolver));
  resolver->Detach();

  EXPECT_CALL(disconnect_callback_, Run());
  source->Close();
}

}  // namespace blink

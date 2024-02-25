// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_source.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
        script_state, WTF::CrossThreadBindOnce(disconnect_callback_.Get()));
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

}  // namespace blink

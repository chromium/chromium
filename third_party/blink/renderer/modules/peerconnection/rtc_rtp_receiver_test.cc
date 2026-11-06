// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"

#include <cstdint>
#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_configuration.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_transceiver_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_mediastreamtrack_string.h"
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_platform.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_script_transform.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_audio_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/make_ref_counted.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace blink {

namespace {

constexpr uint32_t kSsrc = 1;

class MockTransformedFrameCallback : public webrtc::TransformedFrameCallback {
 public:
  MOCK_METHOD(void,
              OnTransformedFrame,
              (std::unique_ptr<webrtc::TransformableFrameInterface>),
              (override));
  MOCK_METHOD(void, StartShortCircuiting, (), (override));
  MOCK_METHOD(void, StopShortCircuiting, (), (override));
};

// Parameterized by the media kind of the receiver.
class RTCRtpReceiverTransformTest : public testing::TestWithParam<const char*> {
 public:
  void TearDown() override {
    // Release the transformer's reference to `sink_callback_`, so that the mock
    // is verified when the fixture is destroyed rather than whenever the
    // receiver gets garbage collected.
    if (!receiver_) {
      return;
    }
    if (IsAudio()) {
      receiver_->platform_receiver()
          ->GetEncodedAudioStreamTransformer()
          ->UnregisterTransformedFrameCallback();
    } else {
      receiver_->platform_receiver()
          ->GetEncodedVideoStreamTransformer()
          ->UnregisterTransformedFrameSinkCallback(kSsrc);
    }
  }

 protected:
  bool IsAudio() const { return String(GetParam()) == "audio"; }

  RTCPeerConnection* CreatePC(V8TestingScope& scope,
                              bool encoded_insertable_streams = false) {
    RTCPeerConnection::SetRtcPeerConnectionHandlerFactoryForTesting(
        base::BindRepeating([]() -> std::unique_ptr<RTCPeerConnectionHandler> {
          return std::make_unique<MockRTCPeerConnectionHandlerPlatform>();
        }));
    RTCConfiguration* config = RTCConfiguration::Create();
    config->setEncodedInsertableStreams(encoded_insertable_streams);
    return RTCPeerConnection::Create(scope.GetExecutionContext(), config,
                                     scope.GetExceptionState());
  }

  // Adds a transceiver of kind GetParam() to `pc`, and registers
  // `sink_callback_` as the WebRTC sink of the encoded transformer of its
  // receiver. The receiver's short circuit task has not run yet when this
  // returns.
  RTCRtpReceiver* CreateReceiverWithSinkCallback(V8TestingScope& scope,
                                                 RTCPeerConnection* pc) {
    RTCRtpTransceiver* transceiver = pc->addTransceiver(
        MakeGarbageCollected<V8UnionMediaStreamTrackOrString>(
            String(GetParam())),
        RTCRtpTransceiverInit::Create(), scope.GetExceptionState());
    CHECK(transceiver);
    receiver_ = pc->getReceivers().back();
    if (IsAudio()) {
      receiver_->platform_receiver()
          ->GetEncodedAudioStreamTransformer()
          ->RegisterTransformedFrameCallback(sink_callback_);
    } else {
      receiver_->platform_receiver()
          ->GetEncodedVideoStreamTransformer()
          ->RegisterTransformedFrameSinkCallback(sink_callback_, kSsrc);
    }
    return receiver_.Get();
  }

  test::TaskEnvironment task_environment_;
  const webrtc::scoped_refptr<testing::StrictMock<MockTransformedFrameCallback>>
      sink_callback_ = webrtc::make_ref_counted<
          testing::StrictMock<MockTransformedFrameCallback>>();
  Persistent<RTCRtpReceiver> receiver_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         RTCRtpReceiverTransformTest,
                         testing::Values("audio", "video"));

// Regression test for crbug.com/581889083: A transform set after the short
// circuit task has run, e.g. in the offerer's ontrack handler, must get frames.
TEST_P(RTCRtpReceiverTransformTest, LateTransformStopsShortCircuiting) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);
  RTCRtpReceiver* receiver = CreateReceiverWithSinkCallback(scope, pc);

  // No transform is set when the short circuit task runs.
  EXPECT_CALL(*sink_callback_, StartShortCircuiting);
  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(sink_callback_.get());

  // Frames must not bypass a transform that is set later.
  EXPECT_CALL(*sink_callback_, StopShortCircuiting);
  auto* transform = MakeGarbageCollected<RTCRtpScriptTransform>();
  receiver->setTransform(transform, scope.GetExceptionState());
  EXPECT_EQ(receiver->transform(), transform);
  testing::Mock::VerifyAndClearExpectations(sink_callback_.get());

  // Replacing the transform keeps frames going through the transform.
  receiver->setTransform(MakeGarbageCollected<RTCRtpScriptTransform>(),
                         scope.GetExceptionState());

  // Without a transform, frames pass through untransformed.
  EXPECT_CALL(*sink_callback_, StartShortCircuiting);
  receiver->setTransform(nullptr, scope.GetExceptionState());
  EXPECT_FALSE(receiver->transform());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_P(RTCRtpReceiverTransformTest, NullTransformStartsShortCircuiting) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);
  RTCRtpReceiver* receiver = CreateReceiverWithSinkCallback(scope, pc);

  // The transform is set before the short circuit task runs, so the task
  // leaves the frames going through the transform.
  receiver->setTransform(MakeGarbageCollected<RTCRtpScriptTransform>(),
                         scope.GetExceptionState());
  test::RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(sink_callback_.get());

  EXPECT_CALL(*sink_callback_, StartShortCircuiting);
  receiver->setTransform(nullptr, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());

  // Encoded streams would not get any frames now.
  EXPECT_FALSE(receiver->createEncodedStreams(scope.GetScriptState(),
                                              scope.GetExceptionState()));
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_P(RTCRtpReceiverTransformTest, TransformClearedBeforeShortCircuitTask) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);
  RTCRtpReceiver* receiver = CreateReceiverWithSinkCallback(scope, pc);

  // Encoded streams can still be created until the short circuit task runs, so
  // clearing the transform leaves short circuiting to that task.
  receiver->setTransform(MakeGarbageCollected<RTCRtpScriptTransform>(),
                         scope.GetExceptionState());
  receiver->setTransform(nullptr, scope.GetExceptionState());
  testing::Mock::VerifyAndClearExpectations(sink_callback_.get());

  EXPECT_CALL(*sink_callback_, StartShortCircuiting);
  test::RunPendingTasks();
}

TEST_P(RTCRtpReceiverTransformTest,
       NoShortCircuitingWhenEncodedInsertableStreamsAreRequired) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope, /*encoded_insertable_streams=*/true);
  ASSERT_TRUE(pc);
  RTCRtpReceiver* receiver = CreateReceiverWithSinkCallback(scope, pc);

  // `sink_callback_` is a StrictMock, so any short circuiting request fails the
  // test.
  test::RunPendingTasks();
  receiver->setTransform(MakeGarbageCollected<RTCRtpScriptTransform>(),
                         scope.GetExceptionState());
  receiver->setTransform(nullptr, scope.GetExceptionState());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST_P(RTCRtpReceiverTransformTest,
       CreateEncodedStreamsStaysTooLateAfterLateTransform) {
  V8TestingScope scope;
  RTCPeerConnection* pc = CreatePC(scope);
  ASSERT_TRUE(pc);
  RTCRtpReceiver* receiver = CreateReceiverWithSinkCallback(scope, pc);

  EXPECT_CALL(*sink_callback_, StartShortCircuiting);
  test::RunPendingTasks();
  EXPECT_CALL(*sink_callback_, StopShortCircuiting);
  receiver->setTransform(MakeGarbageCollected<RTCRtpScriptTransform>(),
                         scope.GetExceptionState());

  EXPECT_FALSE(receiver->createEncodedStreams(scope.GetScriptState(),
                                              scope.GetExceptionState()));
  EXPECT_EQ(scope.GetExceptionState().CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

}  // namespace blink

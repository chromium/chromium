// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/test/mock_transformable_audio_frame.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/api/units/timestamp.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

using webrtc::MockTransformableAudioFrame;

namespace blink {

class RTCEncodedAudioFrameTest : public testing::Test {
  test::TaskEnvironment task_environment_;
};

webrtc::Timestamp GetWebRTCTimeOrigin(LocalDOMWindow* window) {
  return webrtc::Timestamp::Micros(
      (WindowPerformance::GetTimeOrigin(window) - base::TimeTicks())
          .InMicroseconds());
}

void MockMetadata(MockTransformableAudioFrame* frame) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(7));
  std::array<uint32_t, 2> csrcs{6, 4};
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(csrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(13));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(20));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(17));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
}

void MockReceiverMetadata(MockTransformableAudioFrame* frame,
                          LocalDOMWindow* window = nullptr) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(7));
  std::array<uint32_t, 2> csrcs{6, 4};
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(csrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(13));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(20));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(17));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
  if (window) {
    // Mark frame as a receiver frame and set a receive time.
    ON_CALL(*frame, GetDirection())
        .WillByDefault(Return(
            webrtc::TransformableAudioFrameInterface::Direction::kReceiver));
    ON_CALL(*frame, ReceiveTime())
        .WillByDefault(Return(GetWebRTCTimeOrigin(window) +
                              webrtc::TimeDelta::Millis(2000)));
  }
}

RTCEncodedAudioFrameMetadata* CreateAudioMetadata() {
  RTCEncodedAudioFrameMetadata* new_metadata =
      RTCEncodedAudioFrameMetadata::Create();
  new_metadata->setSynchronizationSource(7);
  new_metadata->setContributingSources({6, 4});
  new_metadata->setPayloadType(13);
  new_metadata->setMimeType("image");
  new_metadata->setSequenceNumber(20);
  new_metadata->setRtpTimestamp(110);
  return new_metadata;
}

TEST_F(RTCEncodedAudioFrameTest, GetMetadataReturnsCorrectMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<MockTransformableAudioFrame>();
  MockReceiverMetadata(frame.get(), &v8_scope.GetWindow());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  RTCEncodedAudioFrameMetadata* retrieved_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());
  EXPECT_EQ(7u, retrieved_metadata->synchronizationSource());
  ASSERT_EQ(2u, retrieved_metadata->contributingSources().size());
  EXPECT_EQ(6u, retrieved_metadata->contributingSources()[0]);
  EXPECT_EQ(4u, retrieved_metadata->contributingSources()[1]);
  EXPECT_EQ(13, retrieved_metadata->payloadType());
  EXPECT_EQ("image", retrieved_metadata->mimeType());
  EXPECT_EQ(20u, retrieved_metadata->sequenceNumber());
  EXPECT_EQ(17u, retrieved_metadata->rtpTimestamp());
  EXPECT_TRUE(retrieved_metadata->hasReceiveTime());
  // The precision for DOMHighResTimestamp is 0.1ms. Test equality by making
  // sure the difference between expected and received  is less than 0.2ms.
  EXPECT_LT(std::abs(2000.0 - retrieved_metadata->receiveTime()), 0.2);
}

TEST_F(RTCEncodedAudioFrameTest, SetMetadataOnEmptyFrameFails) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  RTCEncodedAudioFrameMetadata* new_metadata = CreateAudioMetadata();

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(v8_scope.GetExecutionContext(), new_metadata,
                             exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: Invalid modification of "
            "RTCEncodedAudioFrameMetadata. Bad "
            "synchronizationSource");
}

TEST_F(RTCEncodedAudioFrameTest, SetMetadataModifiesRtpTimestamp) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());
  EXPECT_CALL(*frame, SetRTPTimestamp(110)).Times(1);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  EXPECT_EQ(encoded_frame->getMetadata(v8_scope.GetExecutionContext())
                ->rtpTimestamp(),
            17u);
  RTCEncodedAudioFrameMetadata* new_metadata = CreateAudioMetadata();

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(v8_scope.GetExecutionContext(), new_metadata,
                             exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorFromNull) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      v8_scope.GetExecutionContext(), nullptr, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new AudioFrame: input Audioframe is empty.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorOnEmptyFrameHasEmptyMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      execution_context, encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_FALSE(
      new_frame->getMetadata(execution_context)->hasSynchronizationSource());
  EXPECT_EQ(
      new_frame->getMetadata(execution_context)->contributingSources().size(),
      0u);
  EXPECT_FALSE(new_frame->getMetadata(execution_context)->hasPayloadType());
  EXPECT_FALSE(new_frame->getMetadata(execution_context)->hasMimeType());
  EXPECT_FALSE(new_frame->getMetadata(execution_context)->hasSequenceNumber());
  EXPECT_EQ(new_frame->getMetadata(execution_context)->rtpTimestamp(), 0u);
  EXPECT_FALSE(new_frame->getMetadata(execution_context)->hasReceiveTime());
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithMetadataOnEmptyFrameFails) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(CreateAudioMetadata());

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      v8_scope.GetExecutionContext(), encoded_frame, frame_options,
      exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new AudioFrame: Invalid modification of "
            "RTCEncodedAudioFrameMetadata. Bad "
            "synchronizationSource");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedAudioFrameTest,
       ConstructorWithRTPTimestampMetadataOnEmptyFrameFails) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  RTCEncodedAudioFrameMetadata* new_metadata =
      RTCEncodedAudioFrameMetadata::Create();
  new_metadata->setContributingSources({});
  new_metadata->setRtpTimestamp(110);
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      v8_scope.GetExecutionContext(), encoded_frame, frame_options,
      exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(
      exception_state.Message(),
      "Cannot create a new AudioFrame: Underlying webrtc frame doesn't exist.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithMetadataModifiesRtpTimestamp) {
  V8TestingScope v8_scope;
  const uint32_t new_timestamp = 110;
  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  EXPECT_EQ(encoded_frame->getMetadata(execution_context)->rtpTimestamp(), 17u);
  RTCEncodedAudioFrameMetadata* new_metadata =
      encoded_frame->getMetadata(execution_context);
  new_metadata->setRtpTimestamp(new_timestamp);
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      execution_context, encoded_frame, frame_options, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  EXPECT_EQ(new_frame->getMetadata(execution_context)->rtpTimestamp(),
            new_timestamp);
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->rtpTimestamp(),
            new_timestamp);
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorCopiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      execution_context, encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  RTCEncodedAudioFrameMetadata* new_frame_metadata =
      new_frame->getMetadata(execution_context);

  EXPECT_EQ(7u, new_frame_metadata->synchronizationSource());
  ASSERT_EQ(2u, new_frame_metadata->contributingSources().size());
  EXPECT_EQ(6u, new_frame_metadata->contributingSources()[0]);
  EXPECT_EQ(4u, new_frame_metadata->contributingSources()[1]);
  EXPECT_EQ(13, new_frame_metadata->payloadType());
  EXPECT_EQ("image", new_frame_metadata->mimeType());
  EXPECT_EQ(20u, new_frame_metadata->sequenceNumber());
  EXPECT_EQ(17u, new_frame_metadata->rtpTimestamp());
  EXPECT_FALSE(new_frame_metadata->hasReceiveTime());
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithMetadataCopiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  RTCEncodedAudioFrameMetadata* new_metadata = CreateAudioMetadata();
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      execution_context, encoded_frame, frame_options, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  RTCEncodedAudioFrameMetadata* new_frame_metadata =
      new_frame->getMetadata(execution_context);

  EXPECT_EQ(new_metadata->synchronizationSource(),
            new_frame_metadata->synchronizationSource());
  ASSERT_EQ(new_metadata->contributingSources().size(),
            new_frame_metadata->contributingSources().size());
  EXPECT_EQ(new_metadata->contributingSources()[0],
            new_frame_metadata->contributingSources()[0]);
  EXPECT_EQ(new_metadata->contributingSources()[1],
            new_frame_metadata->contributingSources()[1]);
  EXPECT_EQ(new_metadata->payloadType(), new_frame_metadata->payloadType());
  EXPECT_EQ(new_metadata->mimeType(), new_frame_metadata->mimeType());
  EXPECT_EQ(new_metadata->sequenceNumber(),
            new_frame_metadata->sequenceNumber());
  EXPECT_EQ(new_metadata->rtpTimestamp(), new_frame_metadata->rtpTimestamp());
  EXPECT_FALSE(new_metadata->hasReceiveTime());
}

TEST_F(RTCEncodedAudioFrameTest, ReadingDataOnEmptyFrameGivesDetachedFrame) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  DOMArrayBuffer* data = encoded_frame->data(v8_scope.GetExecutionContext());
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(data->IsDetached());
}

TEST_F(RTCEncodedAudioFrameTest, PassWebRTCDetachesFrameData) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  EXPECT_CALL(*frame, SetRTPTimestamp(_)).Times(0);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  DOMArrayBuffer* data = encoded_frame->data(v8_scope.GetExecutionContext());
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/true);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(data->IsDetached());
}

TEST_F(RTCEncodedAudioFrameTest, FrameWithSenderCaptureTimeOffset) {
  V8TestingScope v8_scope;
  double sender_capture_offsets_in_millis[] = {12, -34};
  for (int offset : sender_capture_offsets_in_millis) {
    std::unique_ptr<MockTransformableAudioFrame> frame =
        std::make_unique<NiceMock<MockTransformableAudioFrame>>();
    ON_CALL(*frame, SenderCaptureTimeOffset)
        .WillByDefault(Return(webrtc::TimeDelta::Millis(offset)));

    RTCEncodedAudioFrame* encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
    RTCEncodedAudioFrameMetadata* metadata =
        encoded_frame->getMetadata(v8_scope.GetExecutionContext());
    EXPECT_TRUE(metadata->hasSenderCaptureTimeOffset());
    EXPECT_EQ(metadata->getSenderCaptureTimeOffsetOr(0.0), offset);
  }
}

TEST_F(RTCEncodedAudioFrameTest, FrameWithCaptureTime) {
  V8TestingScope v8_scope;
  auto* performance = DOMWindowPerformance::performance(v8_scope.GetWindow());
  const base::TimeTicks window_time_origin =
      performance->GetTimeOriginInternal();
  const double capture_times_in_millis[] = {12, -34};
  for (int capture_time : capture_times_in_millis) {
    base::TimeDelta ntp_capture_time = base::Milliseconds(capture_time) +
                                       window_time_origin.since_origin() -
                                       WebRTCFrameNtpEpoch().since_origin();
    std::unique_ptr<MockTransformableAudioFrame> frame =
        std::make_unique<NiceMock<MockTransformableAudioFrame>>();
    // Currently, only receiver frames expose captureTime.
    ON_CALL(*frame, GetDirection)
        .WillByDefault(
            Return(webrtc::TransformableFrameInterface::Direction::kReceiver));
    ON_CALL(*frame, CaptureTime)
        .WillByDefault(Return(
            webrtc::Timestamp::Micros(ntp_capture_time.InMicroseconds())));

    RTCEncodedAudioFrame* encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
    RTCEncodedAudioFrameMetadata* metadata =
        encoded_frame->getMetadata(v8_scope.GetExecutionContext());
    EXPECT_TRUE(metadata->hasCaptureTime());
    // The error is slightly more than 0.1; use 0.11 to avoid flakes.
    EXPECT_LE(std::abs(metadata->getCaptureTimeOr(0.0) - capture_time), 0.11);
  }
}

}  // namespace blink

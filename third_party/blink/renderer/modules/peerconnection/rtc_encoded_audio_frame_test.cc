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
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_util.h"
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

constexpr uint32_t kSsrc = 7;
constexpr std::array<uint32_t, 2> kCsrcs{6, 4};
constexpr uint8_t kPayloadType = 13;
constexpr uint16_t kSequenceNumber = 20;
constexpr uint32_t kRtpTimestamp = 17;
constexpr int kReceiveTimeMillis = 2000;
constexpr int kCaptureTimeMillis = 2345;

void MockMetadata(MockTransformableAudioFrame* frame) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(kSsrc));
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(kCsrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(kPayloadType));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(kSequenceNumber));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(kRtpTimestamp));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
  ON_CALL(*frame, CaptureTime())
      .WillByDefault(Return(webrtc::Timestamp::Millis(kCaptureTimeMillis)));
  ON_CALL(*frame, AbsoluteCaptureTimestamp())
      .WillByDefault(Return(kCaptureTimeMillis));
  ON_CALL(*frame, GetDirection())
      .WillByDefault(
          Return(webrtc::TransformableAudioFrameInterface::Direction::kSender));
  ON_CALL(*frame, CanSetPayloadType()).WillByDefault(Return(true));
  ON_CALL(*frame, CanSetCaptureTime()).WillByDefault(Return(true));
}

void MockReceiverMetadata(MockTransformableAudioFrame* frame,
                          LocalDOMWindow& window) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(kSsrc));
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(kCsrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(kPayloadType));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(kSequenceNumber));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(kRtpTimestamp));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
  // Mark frame as a receiver frame and set a receive time.
  ON_CALL(*frame, GetDirection())
      .WillByDefault(Return(
          webrtc::TransformableAudioFrameInterface::Direction::kReceiver));
  ON_CALL(*frame, ReceiveTime())
      .WillByDefault(Return(GetWebRTCTimeOrigin(&window) +
                            webrtc::TimeDelta::Millis(kReceiveTimeMillis)));
}

constexpr uint32_t kRtpTimestamp2 = 110;
constexpr uint8_t kPayloadType2 = 19;
constexpr int kCaptureTimeMillis2 = 5432;

RTCEncodedAudioFrameMetadata* CreateAudioMetadata(
    ExecutionContext* execution_context = nullptr) {
  RTCEncodedAudioFrameMetadata* new_metadata =
      RTCEncodedAudioFrameMetadata::Create();
  new_metadata->setSynchronizationSource(kSsrc);
  new_metadata->setContributingSources({kCsrcs[0], kCsrcs[1]});
  new_metadata->setMimeType("image");
  new_metadata->setSequenceNumber(kSequenceNumber);
  // Settable fields
  new_metadata->setRtpTimestamp(kRtpTimestamp2);
  new_metadata->setPayloadType(kPayloadType2);
  if (execution_context) {
    new_metadata->setCaptureTime(CalculateRTCEncodedFrameTimestamp(
        execution_context,
        base::TimeTicks() + base::Milliseconds(kCaptureTimeMillis2)));
  }
  return new_metadata;
}

TEST_F(RTCEncodedAudioFrameTest, GetMetadataReturnsCorrectMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockReceiverMetadata(frame.get(), v8_scope.GetWindow());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  RTCEncodedAudioFrameMetadata* retrieved_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());
  EXPECT_EQ(kSsrc, retrieved_metadata->synchronizationSource());
  ASSERT_EQ(kCsrcs.size(), retrieved_metadata->contributingSources().size());
  EXPECT_EQ(kCsrcs[0], retrieved_metadata->contributingSources()[0]);
  EXPECT_EQ(kCsrcs[1], retrieved_metadata->contributingSources()[1]);
  EXPECT_EQ(kPayloadType, retrieved_metadata->payloadType());
  EXPECT_EQ("image", retrieved_metadata->mimeType());
  EXPECT_EQ(kSequenceNumber, retrieved_metadata->sequenceNumber());
  EXPECT_EQ(kRtpTimestamp, retrieved_metadata->rtpTimestamp());
  EXPECT_TRUE(retrieved_metadata->hasReceiveTime());
  // The precision for DOMHighResTimestamp is 0.1ms. Test equality by making
  // sure the difference between expected and received  is less than 0.2ms.
  EXPECT_LT(std::abs(kReceiveTimeMillis - retrieved_metadata->receiveTime()),
            0.2);
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

TEST_F(RTCEncodedAudioFrameTest, SetMetadataModifiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());
  EXPECT_CALL(*frame, SetRTPTimestamp(kRtpTimestamp2)).Times(1);
  EXPECT_CALL(*frame, SetPayloadType(kPayloadType2)).Times(1);
  EXPECT_CALL(*frame, SetCaptureTime(_)).Times(1);

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  EXPECT_EQ(encoded_frame->getMetadata(v8_scope.GetExecutionContext())
                ->rtpTimestamp(),
            kRtpTimestamp);
  RTCEncodedAudioFrameMetadata* new_metadata =
      CreateAudioMetadata(v8_scope.GetExecutionContext());

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
  EXPECT_CALL(*frame, SetPayloadType(_)).Times(0);
  EXPECT_CALL(*frame, SetCaptureTime(_)).Times(0);

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
  EXPECT_CALL(*frame, SetPayloadType(_)).Times(0);
  EXPECT_CALL(*frame, SetCaptureTime(_)).Times(0);

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
  EXPECT_CALL(*frame, SetPayloadType(_)).Times(0);
  EXPECT_CALL(*frame, SetCaptureTime(_)).Times(0);

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

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithMetadataModifiesMetadata) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  EXPECT_EQ(encoded_frame->getMetadata(execution_context)->rtpTimestamp(), 17u);
  RTCEncodedAudioFrameMetadata* new_metadata =
      encoded_frame->getMetadata(execution_context);
  new_metadata->setRtpTimestamp(kRtpTimestamp2);
  new_metadata->setPayloadType(kPayloadType2);
  new_metadata->setCaptureTime(kCaptureTimeMillis2);
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      execution_context, encoded_frame, frame_options, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  EXPECT_EQ(new_frame->getMetadata(execution_context)->rtpTimestamp(),
            kRtpTimestamp2);
  EXPECT_EQ(new_frame->getMetadata(execution_context)->payloadType(),
            kPayloadType2);
  EXPECT_LE(std::abs(new_frame->getMetadata(execution_context)->captureTime() -
                     kCaptureTimeMillis2),
            1.0);
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->rtpTimestamp(),
            new_frame->getMetadata(execution_context)->rtpTimestamp());
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->payloadType(),
            new_frame->getMetadata(execution_context)->payloadType());
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->captureTime(),
            new_frame->getMetadata(execution_context)->captureTime());
}

double ToDOMHighResTimestamp(ExecutionContext* execution_context, int millis) {
  return CalculateRTCEncodedFrameTimestamp(
      execution_context, base::TimeTicks() + base::Milliseconds(millis));
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

  EXPECT_EQ(kSsrc, new_frame_metadata->synchronizationSource());
  ASSERT_EQ(kCsrcs.size(), new_frame_metadata->contributingSources().size());
  EXPECT_EQ(kCsrcs[0], new_frame_metadata->contributingSources()[0]);
  EXPECT_EQ(kCsrcs[1], new_frame_metadata->contributingSources()[1]);
  EXPECT_EQ(kPayloadType, new_frame_metadata->payloadType());
  EXPECT_EQ("image", new_frame_metadata->mimeType());
  EXPECT_EQ(kSequenceNumber, new_frame_metadata->sequenceNumber());
  EXPECT_EQ(kRtpTimestamp, new_frame_metadata->rtpTimestamp());
  EXPECT_EQ(
      ToDOMHighResTimestamp(v8_scope.GetExecutionContext(), kCaptureTimeMillis),
      new_frame_metadata->captureTime());
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
  RTCEncodedAudioFrameMetadata* new_metadata =
      CreateAudioMetadata(v8_scope.GetExecutionContext());
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
  EXPECT_EQ(new_metadata->captureTime(), new_frame_metadata->captureTime());
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

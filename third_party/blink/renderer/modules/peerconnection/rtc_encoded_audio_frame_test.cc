// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include <cstdint>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/types/expected.h"
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
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
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
namespace {

class RTCEncodedAudioFrameTest : public testing::Test {
  test::TaskEnvironment task_environment_;
};

webrtc::Timestamp GetWebRTCTimeOrigin(LocalDOMWindow* window) {
  return webrtc::Timestamp::Micros(
      (WindowPerformance::GetTimeOrigin(window) - base::TimeTicks())
          .InMicroseconds());
}

DOMHighResTimeStamp GetTimeOriginNtp(V8TestingScope& v8_scope) {
  return DOMWindowPerformance::performance(v8_scope.GetWindow())->timeOrigin() +
         2208988800000.0;
}

constexpr uint32_t kSsrc = 7;
constexpr std::array<uint32_t, 2> kCsrcs{6, 4};
constexpr uint8_t kPayloadType = 13;
constexpr uint16_t kSequenceNumber = 20;
constexpr uint8_t kAudioLevel_dBov = 65;
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
  ON_CALL(*frame, AudioLevel()).WillByDefault(Return(kAudioLevel_dBov));
  ON_CALL(*frame, CanSetPayloadType()).WillByDefault(Return(true));
  ON_CALL(*frame, CanSetCaptureTime()).WillByDefault(Return(true));
  ON_CALL(*frame, CanSetAudioLevel()).WillByDefault(Return(true));
}

void MockReceiverMetadata(MockTransformableAudioFrame* frame,
                          LocalDOMWindow& window) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(kSsrc));
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(kCsrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(kPayloadType));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(kSequenceNumber));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(kRtpTimestamp));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
  ON_CALL(*frame, CaptureTime())
      .WillByDefault(Return(webrtc::Timestamp::Millis(kCaptureTimeMillis)));
  // Mark frame as a receiver frame and set a receive time.
  ON_CALL(*frame, GetDirection())
      .WillByDefault(Return(
          webrtc::TransformableAudioFrameInterface::Direction::kReceiver));
  ON_CALL(*frame, ReceiveTime())
      .WillByDefault(Return(GetWebRTCTimeOrigin(&window) +
                            webrtc::TimeDelta::Millis(kReceiveTimeMillis)));
  ON_CALL(*frame, AudioLevel()).WillByDefault(Return(kAudioLevel_dBov));
}

constexpr uint32_t kRtpTimestamp2 = 110;
constexpr uint8_t kPayloadType2 = 19;
constexpr int kCaptureTimeMillis2 = 5432;
constexpr uint8_t kAudioLevel_dBov2 = 99;

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
  new_metadata->setAudioLevel(ToLinearAudioLevel(kAudioLevel_dBov2));
  if (execution_context) {
    new_metadata->setCaptureTime(RTCEncodedFrameTimestampFromCaptureTimeInfo(
        execution_context,
        {.capture_time = base::Milliseconds(kCaptureTimeMillis2),
         .clock_type = CaptureTimeInfo::ClockType::kTimeTicks}));
  }
  return new_metadata;
}

bool AreMetadataEqual(RTCEncodedAudioFrameMetadata* m1,
                      RTCEncodedAudioFrameMetadata* m2) {
  return m1->hasSequenceNumber() == m2->hasSequenceNumber() &&
         (!m1->hasSequenceNumber() ||
          m1->sequenceNumber() == m2->sequenceNumber()) &&
         m1->hasRtpTimestamp() == m2->hasRtpTimestamp() &&
         (!m1->hasRtpTimestamp() || m1->rtpTimestamp() == m2->rtpTimestamp()) &&
         m1->hasCaptureTime() == m2->hasCaptureTime() &&
         (!m1->hasCaptureTime() || m1->captureTime() == m2->captureTime()) &&
         m1->hasSenderCaptureTimeOffset() == m2->hasSenderCaptureTimeOffset() &&
         (!m1->hasSenderCaptureTimeOffset() ||
          m1->senderCaptureTimeOffset() == m2->senderCaptureTimeOffset()) &&
         m1->hasReceiveTime() == m2->hasReceiveTime() &&
         (!m1->hasReceiveTime() || m1->receiveTime() == m2->receiveTime()) &&
         m1->hasMimeType() == m2->hasMimeType() &&
         (!m1->hasMimeType() || m1->mimeType() == m2->mimeType()) &&
         m1->hasPayloadType() == m2->hasPayloadType() &&
         (!m1->hasPayloadType() || m1->payloadType() == m2->payloadType()) &&
         m1->hasContributingSources() == m2->hasContributingSources() &&
         (!m1->hasContributingSources() ||
          (m1->contributingSources() == m2->contributingSources())) &&
         m1->hasSynchronizationSource() == m2->hasSynchronizationSource() &&
         (!m1->hasSynchronizationSource() ||
          (m1->synchronizationSource() == m2->synchronizationSource())) &&
         m1->hasAudioLevel() == m2->hasAudioLevel() &&
         (!m1->hasAudioLevel() || m1->audioLevel() == m2->audioLevel());
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
  EXPECT_EQ(ToLinearAudioLevel(kAudioLevel_dBov),
            retrieved_metadata->audioLevel());
}

TEST_F(RTCEncodedAudioFrameTest, SetCaptureTimeOnReceiverFrame) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockReceiverMetadata(frame.get(), v8_scope.GetWindow());
  ASSERT_FALSE(frame->CanSetCaptureTime());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  RTCEncodedAudioFrameMetadata* new_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());
  ASSERT_TRUE(new_metadata->hasCaptureTime());
  double original_capture_time = new_metadata->captureTime();

  // Small differences in captureTime are ignored.
  new_metadata->setCaptureTime(original_capture_time + 0.01);
  base::expected<void, String> result =
      encoded_frame->SetMetadata(v8_scope.GetExecutionContext(), new_metadata);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(
      encoded_frame->getMetadata(v8_scope.GetExecutionContext())->captureTime(),
      original_capture_time);

  new_metadata->setCaptureTime(original_capture_time - 0.01);
  result =
      encoded_frame->SetMetadata(v8_scope.GetExecutionContext(), new_metadata);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(
      encoded_frame->getMetadata(v8_scope.GetExecutionContext())->captureTime(),
      original_capture_time);

  // Significantly different capture times cannot be set.
  new_metadata->setCaptureTime(1234);
  result =
      encoded_frame->SetMetadata(v8_scope.GetExecutionContext(), new_metadata);
  EXPECT_FALSE(result.has_value());
}

TEST_F(RTCEncodedAudioFrameTest,
       SetCaptureTimeOnReceiverFrameWithoutCaptureTime) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockReceiverMetadata(frame.get(), v8_scope.GetWindow());
  ON_CALL(*frame, CaptureTime()).WillByDefault(Return(std::nullopt));
  ASSERT_FALSE(frame->CanSetCaptureTime());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  RTCEncodedAudioFrameMetadata* new_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());
  ASSERT_FALSE(new_metadata->hasCaptureTime());

  base::expected<void, String> result =
      encoded_frame->SetMetadata(v8_scope.GetExecutionContext(), new_metadata);
  EXPECT_TRUE(result.has_value());
  EXPECT_FALSE(encoded_frame->getMetadata(v8_scope.GetExecutionContext())
                   ->hasCaptureTime());
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
}

TEST_F(RTCEncodedAudioFrameTest, SetMetadataModifiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());
  EXPECT_CALL(*frame, SetRTPTimestamp(kRtpTimestamp2)).Times(1);
  EXPECT_CALL(*frame, SetPayloadType(kPayloadType2)).Times(1);
  EXPECT_CALL(*frame, SetCaptureTime(_)).Times(1);
  EXPECT_CALL(*frame, SetAudioLevel(std::make_optional(kAudioLevel_dBov2)))
      .Times(1);

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
  EXPECT_CALL(*frame, SetAudioLevel(_)).Times(0);

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
  EXPECT_CALL(*frame, SetAudioLevel(_)).Times(0);

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
  EXPECT_CALL(*frame, SetAudioLevel(_)).Times(0);

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
  new_metadata->setAudioLevel(ToLinearAudioLevel(kAudioLevel_dBov2));
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
  EXPECT_EQ(new_frame->getMetadata(execution_context)->audioLevel(),
            ToLinearAudioLevel(kAudioLevel_dBov2));

  EXPECT_NE(encoded_frame->getMetadata(execution_context)->rtpTimestamp(),
            new_frame->getMetadata(execution_context)->rtpTimestamp());
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->payloadType(),
            new_frame->getMetadata(execution_context)->payloadType());
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->captureTime(),
            new_frame->getMetadata(execution_context)->captureTime());
  EXPECT_NE(encoded_frame->getMetadata(execution_context)->audioLevel(),
            new_frame->getMetadata(execution_context)->audioLevel());
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithInvalidPayloadTypeFails) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());
  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  std::vector<uint8_t> invalid_payload_types;
  // This range is reserved for RCTP.
  // See https://tools.ietf.org/html/rfc5761#section-4
  for (int i = 64; i <= 95; i++) {
    invalid_payload_types.push_back(i);
  }
  // PayloadType is a 7-bit field, so anything > 127 is invalid.
  for (int i = 128; i <= 255; i++) {
    invalid_payload_types.push_back(i);
  }
  for (auto invalid_pt : invalid_payload_types) {
    RTCEncodedAudioFrameMetadata* new_metadata =
        encoded_frame->getMetadata(execution_context);
    new_metadata->setPayloadType(invalid_pt);
    RTCEncodedAudioFrameOptions* frame_options =
        RTCEncodedAudioFrameOptions::Create();
    frame_options->setMetadata(new_metadata);

    DummyExceptionStateForTesting exception_state;
    RTCEncodedAudioFrame::Create(execution_context, encoded_frame,
                                 frame_options, exception_state);
    EXPECT_TRUE(exception_state.HadException());
    EXPECT_EQ(exception_state.Code(),
              static_cast<int>(DOMExceptionCode::kInvalidModificationError));
  }
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithValidPayloadTypeSucceeds) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());
  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  ExecutionContext* execution_context = v8_scope.GetExecutionContext();
  std::vector<uint8_t> valid_payload_types;
  // This range is reserved for RCTP.
  // See https://tools.ietf.org/html/rfc5761#section-4
  for (int i = 0; i <= 63; i++) {
    valid_payload_types.push_back(i);
  }
  for (int i = 96; i <= 127; i++) {
    valid_payload_types.push_back(i);
  }
  for (auto pt : valid_payload_types) {
    RTCEncodedAudioFrameMetadata* new_metadata =
        encoded_frame->getMetadata(execution_context);
    new_metadata->setPayloadType(pt);
    RTCEncodedAudioFrameOptions* frame_options =
        RTCEncodedAudioFrameOptions::Create();
    frame_options->setMetadata(new_metadata);

    DummyExceptionStateForTesting exception_state;
    RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
        execution_context, encoded_frame, frame_options, exception_state);
    EXPECT_FALSE(exception_state.HadException());
    EXPECT_TRUE(new_frame->getMetadata(execution_context)->hasPayloadType());
    EXPECT_EQ(new_frame->getMetadata(execution_context)->payloadType(), pt);
  }
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
  EXPECT_EQ(RTCEncodedFrameTimestampFromCaptureTimeInfo(
                v8_scope.GetExecutionContext(),
                {.capture_time = base::Milliseconds(kCaptureTimeMillis),
                 .clock_type = CaptureTimeInfo::ClockType::kTimeTicks}),
            new_frame_metadata->captureTime());
  EXPECT_EQ(ToLinearAudioLevel(kAudioLevel_dBov),
            new_frame_metadata->audioLevel());
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
  EXPECT_EQ(new_metadata->audioLevel(), new_frame_metadata->audioLevel());
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
  int sender_capture_offsets_in_millis[] = {12, -34};
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

TEST_F(RTCEncodedAudioFrameTest, ReceiverFrameWithCaptureTime) {
  V8TestingScope v8_scope;
  const int capture_times_in_millis[] = {12, -34};
  for (int capture_time : capture_times_in_millis) {
    base::TimeDelta ntp_capture_time =
        base::Milliseconds(GetTimeOriginNtp(v8_scope) + capture_time);
    std::unique_ptr<MockTransformableAudioFrame> frame =
        std::make_unique<NiceMock<MockTransformableAudioFrame>>();
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
    // The error is slightly more than 0.1; use 0.2 to avoid flakes.
    EXPECT_LE(std::abs(metadata->captureTime() - capture_time), 0.2);
  }
}

TEST_F(RTCEncodedAudioFrameTest, SenderFrameWithCaptureTime) {
  V8TestingScope v8_scope;
  const int capture_times_in_millis[] = {12, -34};
  for (int capture_time : capture_times_in_millis) {
    std::unique_ptr<MockTransformableAudioFrame> frame =
        std::make_unique<NiceMock<MockTransformableAudioFrame>>();
    ON_CALL(*frame, GetDirection)
        .WillByDefault(
            Return(webrtc::TransformableFrameInterface::Direction::kSender));
    ON_CALL(*frame, CaptureTime)
        .WillByDefault(Return(GetWebRTCTimeOrigin(&v8_scope.GetWindow()) +
                              webrtc::TimeDelta::Millis(capture_time)));

    RTCEncodedAudioFrame* encoded_frame =
        MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
    RTCEncodedAudioFrameMetadata* metadata =
        encoded_frame->getMetadata(v8_scope.GetExecutionContext());
    EXPECT_TRUE(metadata->hasCaptureTime());
    // The error is slightly more than 0.1; use 0.2 to avoid flakes.
    EXPECT_LE(std::abs(metadata->captureTime() - capture_time), 0.2);
  }
}

TEST_F(RTCEncodedAudioFrameTest, FrameWithAudioLevel) {
  V8TestingScope v8_scope;
  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  ON_CALL(*frame, AudioLevel).WillByDefault(Return(kAudioLevel_dBov));

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  RTCEncodedAudioFrameMetadata* metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());
  EXPECT_TRUE(metadata->hasAudioLevel());
  EXPECT_EQ(metadata->audioLevel(), ToLinearAudioLevel(kAudioLevel_dBov));
}

TEST_F(RTCEncodedAudioFrameTest,
       ReadingMetadataOnEmptyFrameReturnsOriginalMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  RTCEncodedAudioFrameMetadata* original_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());

  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  RTCEncodedAudioFrameMetadata* post_neuter_metadata =
      encoded_frame->getMetadata(v8_scope.GetExecutionContext());

  EXPECT_TRUE(AreMetadataEqual(original_metadata, post_neuter_metadata));
}

}  // namespace
}  // namespace blink

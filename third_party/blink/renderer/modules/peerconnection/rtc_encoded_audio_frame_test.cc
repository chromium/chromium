// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_audio_frame_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_audio_frame_delegate.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/test/mock_transformable_audio_frame.h"

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

void MockMetadata(MockTransformableAudioFrame* frame) {
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(7));
  std::array<uint32_t, 2> csrcs{6, 4};
  ON_CALL(*frame, GetContributingSources()).WillByDefault(Return(csrcs));
  ON_CALL(*frame, GetPayloadType()).WillByDefault(Return(13));
  ON_CALL(*frame, SequenceNumber()).WillByDefault(Return(20));
  ON_CALL(*frame, AbsoluteCaptureTimestamp()).WillByDefault(Return(70050));
  ON_CALL(*frame, GetTimestamp()).WillByDefault(Return(17));
  ON_CALL(*frame, GetMimeType()).WillByDefault(Return("image"));
}

RTCEncodedAudioFrameMetadata* CreateAudioMetadata() {
  RTCEncodedAudioFrameMetadata* new_metadata =
      RTCEncodedAudioFrameMetadata::Create();
  new_metadata->setSynchronizationSource(7);
  new_metadata->setContributingSources({6, 4});
  new_metadata->setPayloadType(13);
  new_metadata->setMimeType("image");
  new_metadata->setSequenceNumber(20);
  new_metadata->setAbsCaptureTime(70050);
  new_metadata->setRtpTimestamp(110);
  return new_metadata;
}

TEST_F(RTCEncodedAudioFrameTest, GetMetadataReturnsCorrectMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<MockTransformableAudioFrame>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));

  RTCEncodedAudioFrameMetadata* retrieved_metadata =
      encoded_frame->getMetadata();
  EXPECT_EQ(7u, retrieved_metadata->synchronizationSource());
  ASSERT_EQ(2u, retrieved_metadata->contributingSources().size());
  EXPECT_EQ(6u, retrieved_metadata->contributingSources()[0]);
  EXPECT_EQ(4u, retrieved_metadata->contributingSources()[1]);
  EXPECT_EQ(13, retrieved_metadata->payloadType());
  EXPECT_EQ("image", retrieved_metadata->mimeType());
  EXPECT_EQ(20u, retrieved_metadata->sequenceNumber());
  EXPECT_EQ(70050u, retrieved_metadata->absCaptureTime());
  EXPECT_EQ(17u, retrieved_metadata->rtpTimestamp());
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
  encoded_frame->setMetadata(new_metadata, exception_state);
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

  EXPECT_EQ(encoded_frame->getMetadata()->rtpTimestamp(), 17u);
  RTCEncodedAudioFrameMetadata* new_metadata = CreateAudioMetadata();

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorFromNull) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame =
      RTCEncodedAudioFrame::Create(nullptr, exception_state);

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

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame =
      RTCEncodedAudioFrame::Create(encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_FALSE(new_frame->getMetadata()->hasSynchronizationSource());
  EXPECT_EQ(new_frame->getMetadata()->contributingSources().size(), 0u);
  EXPECT_FALSE(new_frame->getMetadata()->hasPayloadType());
  EXPECT_FALSE(new_frame->getMetadata()->hasMimeType());
  EXPECT_FALSE(new_frame->getMetadata()->hasSequenceNumber());
  EXPECT_FALSE(new_frame->getMetadata()->hasAbsCaptureTime());
  EXPECT_EQ(new_frame->getMetadata()->rtpTimestamp(), 0u);
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
      encoded_frame, frame_options, exception_state);

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
      encoded_frame, frame_options, exception_state);

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

  EXPECT_EQ(encoded_frame->getMetadata()->rtpTimestamp(), 17u);
  RTCEncodedAudioFrameMetadata* new_metadata = encoded_frame->getMetadata();
  new_metadata->setRtpTimestamp(new_timestamp);
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      encoded_frame, frame_options, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  EXPECT_EQ(new_frame->getMetadata()->rtpTimestamp(), new_timestamp);
  EXPECT_NE(encoded_frame->getMetadata()->rtpTimestamp(), new_timestamp);
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorCopiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrame* new_frame =
      RTCEncodedAudioFrame::Create(encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  RTCEncodedAudioFrameMetadata* new_frame_metadata = new_frame->getMetadata();

  EXPECT_EQ(7u, new_frame_metadata->synchronizationSource());
  ASSERT_EQ(2u, new_frame_metadata->contributingSources().size());
  EXPECT_EQ(6u, new_frame_metadata->contributingSources()[0]);
  EXPECT_EQ(4u, new_frame_metadata->contributingSources()[1]);
  EXPECT_EQ(13, new_frame_metadata->payloadType());
  EXPECT_EQ("image", new_frame_metadata->mimeType());
  EXPECT_EQ(20u, new_frame_metadata->sequenceNumber());
  EXPECT_EQ(70050u, new_frame_metadata->absCaptureTime());
  EXPECT_EQ(17u, new_frame_metadata->rtpTimestamp());
}

TEST_F(RTCEncodedAudioFrameTest, ConstructorWithMetadataCopiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableAudioFrame> frame =
      std::make_unique<NiceMock<MockTransformableAudioFrame>>();
  MockMetadata(frame.get());

  RTCEncodedAudioFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedAudioFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;
  RTCEncodedAudioFrameMetadata* new_metadata = CreateAudioMetadata();
  RTCEncodedAudioFrameOptions* frame_options =
      RTCEncodedAudioFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  RTCEncodedAudioFrame* new_frame = RTCEncodedAudioFrame::Create(
      encoded_frame, frame_options, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  RTCEncodedAudioFrameMetadata* new_frame_metadata = new_frame->getMetadata();

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
  EXPECT_EQ(new_metadata->absCaptureTime(),
            new_frame_metadata->absCaptureTime());
  EXPECT_EQ(new_metadata->rtpTimestamp(), new_frame_metadata->rtpTimestamp());
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

}  // namespace blink

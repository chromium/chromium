// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_options.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

using webrtc::MockTransformableVideoFrame;

namespace blink {

class RTCEncodedVideoFrameTest : public testing::Test {
  test::TaskEnvironment task_environment_;
};

webrtc::VideoFrameMetadata MockVP9Metadata(MockTransformableVideoFrame* frame) {
  webrtc::VideoFrameMetadata webrtc_metadata;
  std::vector<webrtc::DecodeTargetIndication> decode_target_indications;
  decode_target_indications.push_back(
      webrtc::DecodeTargetIndication::kRequired);
  webrtc_metadata.SetDecodeTargetIndications(decode_target_indications);
  webrtc_metadata.SetIsLastFrameInPicture(true);
  webrtc_metadata.SetSimulcastIdx(5);
  webrtc_metadata.SetFrameType(webrtc::VideoFrameType::kVideoFrameKey);
  webrtc_metadata.SetCodec(webrtc::VideoCodecType::kVideoCodecVP9);
  webrtc_metadata.SetFrameId(1);
  webrtc::RTPVideoHeaderVP9 webrtc_vp9_specifics;
  webrtc_vp9_specifics.InitRTPVideoHeaderVP9();
  webrtc_vp9_specifics.inter_pic_predicted = true;
  webrtc_vp9_specifics.flexible_mode = true;
  webrtc_vp9_specifics.beginning_of_frame = true;
  webrtc_metadata.SetRTPVideoHeaderCodecSpecifics(webrtc_vp9_specifics);

  ON_CALL(*frame, Metadata()).WillByDefault(Return(webrtc_metadata));

  return webrtc_metadata;
}

webrtc::VideoFrameMetadata MockVP8Metadata(MockTransformableVideoFrame* frame) {
  webrtc::VideoFrameMetadata webrtc_metadata;
  webrtc_metadata.SetFrameId(2);
  webrtc_metadata.SetFrameDependencies(std::vector<int64_t>{1});
  webrtc_metadata.SetWidth(800);
  webrtc_metadata.SetHeight(600);
  webrtc_metadata.SetSpatialIndex(3);
  webrtc_metadata.SetTemporalIndex(4);
  std::vector<webrtc::DecodeTargetIndication> decode_target_indications;
  decode_target_indications.push_back(
      webrtc::DecodeTargetIndication::kRequired);
  webrtc_metadata.SetDecodeTargetIndications(decode_target_indications);
  webrtc_metadata.SetIsLastFrameInPicture(true);
  webrtc_metadata.SetSimulcastIdx(5);
  webrtc_metadata.SetFrameType(webrtc::VideoFrameType::kVideoFrameKey);
  webrtc_metadata.SetCodec(webrtc::VideoCodecType::kVideoCodecVP8);
  webrtc_metadata.SetCsrcs({6});
  webrtc_metadata.SetSsrc(7);

  webrtc::RTPVideoHeaderVP8 webrtc_vp8_specifics;
  webrtc_vp8_specifics.nonReference = true;
  webrtc_vp8_specifics.pictureId = 8;
  webrtc_vp8_specifics.tl0PicIdx = 9;
  webrtc_vp8_specifics.temporalIdx = 10;
  webrtc_vp8_specifics.layerSync = true;
  webrtc_vp8_specifics.keyIdx = 11;
  webrtc_vp8_specifics.partitionId = 12;
  webrtc_vp8_specifics.beginningOfPartition = true;
  webrtc_metadata.SetRTPVideoHeaderCodecSpecifics(webrtc_vp8_specifics);

  ON_CALL(*frame, Metadata()).WillByDefault(Return(webrtc_metadata));
  ON_CALL(*frame, GetSsrc()).WillByDefault(Return(7));

  return webrtc_metadata;
}

TEST_F(RTCEncodedVideoFrameTest, GetMetadataReturnsMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<MockTransformableVideoFrame>();

  webrtc::VideoFrameMetadata webrtc_metadata = MockVP8Metadata(frame.get());

  EXPECT_CALL(*frame, Metadata()).WillOnce(Return(webrtc_metadata));
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(13));
  EXPECT_CALL(*frame, GetTimestamp()).WillRepeatedly(Return(17));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  RTCEncodedVideoFrameMetadata* retrieved_metadata =
      encoded_frame->getMetadata();
  EXPECT_EQ(7u, retrieved_metadata->synchronizationSource());
  EXPECT_EQ(13, retrieved_metadata->payloadType());
  EXPECT_EQ(2, retrieved_metadata->frameId());
  ASSERT_EQ(1u, retrieved_metadata->dependencies().size());
  EXPECT_EQ(1, retrieved_metadata->dependencies()[0]);
  EXPECT_EQ(800, retrieved_metadata->width());
  EXPECT_EQ(600, retrieved_metadata->height());
  EXPECT_EQ(3, retrieved_metadata->spatialIndex());
  EXPECT_EQ(4, retrieved_metadata->temporalIndex());
  ASSERT_EQ(1u, retrieved_metadata->contributingSources().size());
  EXPECT_EQ(6u, retrieved_metadata->contributingSources()[0]);
  EXPECT_EQ(17u, retrieved_metadata->rtpTimestamp());
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataPreservesVP9CodecSpecifics) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  webrtc::VideoFrameMetadata webrtc_metadata = MockVP9Metadata(frame.get());

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).WillOnce(SaveArg<0>(&actual_metadata));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;

  encoded_frame->setMetadata(encoded_frame->getMetadata(), exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();

  EXPECT_EQ(actual_metadata.GetFrameId(), webrtc_metadata.GetFrameId());
  EXPECT_EQ(actual_metadata.GetRTPVideoHeaderCodecSpecifics(),
            webrtc_metadata.GetRTPVideoHeaderCodecSpecifics());
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataMissingFieldsFails) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  RTCEncodedVideoFrameMetadata* empty_metadata =
      RTCEncodedVideoFrameMetadata::Create();

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(empty_metadata, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: new metadata has member(s) missing.");
}

RTCEncodedVideoFrameMetadata* CreateMetadata(bool change_all_fields = false) {
  RTCEncodedVideoFrameMetadata* new_metadata =
      RTCEncodedVideoFrameMetadata::Create();
  new_metadata->setFrameId(5);
  new_metadata->setDependencies({2, 3, 4});
  new_metadata->setRtpTimestamp(1);
  if (change_all_fields) {
    new_metadata->setWidth(6);
    new_metadata->setHeight(7);
    new_metadata->setSpatialIndex(8);
    new_metadata->setTemporalIndex(9);
    new_metadata->setSynchronizationSource(10);
    new_metadata->setContributingSources({11, 12, 13});
    new_metadata->setPayloadType(14);
  } else {
    new_metadata->setWidth(800);
    new_metadata->setHeight(600);
    new_metadata->setSpatialIndex(3);
    new_metadata->setTemporalIndex(4);
    new_metadata->setSynchronizationSource(7);
    new_metadata->setContributingSources({6});
    new_metadata->setPayloadType(1);
  }
  return new_metadata;
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataWithoutFeatureFailsModifications) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(1));

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  RTCEncodedVideoFrameMetadata* new_metadata =
      CreateMetadata(/*change_all_fields=*/true);

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: invalid modification of "
            "RTCEncodedVideoFrameMetadata.");
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataWithFeatureAllowsModifications) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).WillOnce(SaveArg<0>(&actual_metadata));
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(14));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  RTCEncodedVideoFrameMetadata* new_metadata =
      CreateMetadata(/*change_all_fields=*/true);

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();

  EXPECT_EQ(actual_metadata.GetFrameId(), new_metadata->frameId());
  Vector<int64_t> actual_dependencies;
  for (const auto& dependency : actual_metadata.GetFrameDependencies()) {
    actual_dependencies.push_back(dependency);
  }
  EXPECT_EQ(actual_dependencies, new_metadata->dependencies());
  EXPECT_EQ(actual_metadata.GetWidth(), new_metadata->width());
  EXPECT_EQ(actual_metadata.GetHeight(), new_metadata->height());
  EXPECT_EQ(actual_metadata.GetSpatialIndex(), new_metadata->spatialIndex());
  EXPECT_EQ(actual_metadata.GetTemporalIndex(), new_metadata->temporalIndex());
  EXPECT_EQ(actual_metadata.GetSsrc(), new_metadata->synchronizationSource());
  Vector<uint32_t> actual_csrcs;
  for (const auto& dependency : actual_metadata.GetCsrcs()) {
    actual_csrcs.push_back(dependency);
  }
  EXPECT_EQ(actual_csrcs, new_metadata->contributingSources());
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataOnEmptyFrameFails) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* metadata = encoded_frame->getMetadata();

  // Move the WebRTC frame out, as if the frame had been written into
  // an encoded insertable stream's WritableStream to be sent on.
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(metadata, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: underlying webrtc frame is an empty frame.");
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataRejectsInvalidDependencies) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* new_metadata = CreateMetadata();
  // Set an invalid dependency - all deps must be less than frame id.
  new_metadata->setDependencies({new_metadata->frameId()});

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: new metadata has invalid frame "
            "dependencies.");
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataRejectsTooEarlyDependencies) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* new_metadata = CreateMetadata();
  // Set an invalid dependency - deps must be within 1 << 14 of the frame id.
  new_metadata->setFrameId(1 << 14);
  new_metadata->setDependencies({0});

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: new metadata has invalid frame "
            "dependencies.");
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataRejectsTooManyDependencies) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* new_metadata = CreateMetadata();
  // Set too many dependencies.
  new_metadata->setDependencies({1, 2, 3, 4, 5, 6, 7, 8, 9});

  DummyExceptionStateForTesting exception_state;
  encoded_frame->setMetadata(new_metadata, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot setMetadata: new metadata has too many dependencies.");
}

TEST_F(RTCEncodedVideoFrameTest, SetMetadataModifiesRtpTimestamp) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  const uint32_t new_timestamp = 7;

  EXPECT_CALL(*frame, GetTimestamp()).WillRepeatedly(Return(1));

  EXPECT_CALL(*frame, SetMetadata(_));
  EXPECT_CALL(*frame, SetRTPTimestamp(new_timestamp));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* metadata = encoded_frame->getMetadata();
  metadata->setRtpTimestamp(new_timestamp);

  DummyExceptionStateForTesting exception_state;

  encoded_frame->setMetadata(metadata, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorPreservesVP9CodecSpecifics) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  webrtc::VideoFrameMetadata webrtc_metadata = MockVP9Metadata(frame.get());

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;

  RTCEncodedVideoFrame* new_frame =
      RTCEncodedVideoFrame::Create(encoded_frame, exception_state);
  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();
  EXPECT_EQ(new_frame->getMetadata()->frameId(), webrtc_metadata.GetFrameId());
  EXPECT_EQ(new_frame->getMetadata()->width(), webrtc_metadata.GetWidth());
  EXPECT_EQ(new_frame->getMetadata()->height(), webrtc_metadata.GetHeight());
  EXPECT_EQ(new_frame->getMetadata()->spatialIndex(),
            webrtc_metadata.GetSpatialIndex());
  EXPECT_EQ(new_frame->getMetadata()->temporalIndex(),
            webrtc_metadata.GetTemporalIndex());
  EXPECT_EQ(new_frame->getMetadata()->synchronizationSource(),
            webrtc_metadata.GetSsrc());
  std::vector<uint32_t> actual_csrcs;
  for (const auto& dependency :
       new_frame->getMetadata()->contributingSources()) {
    actual_csrcs.push_back(dependency);
  }
  EXPECT_EQ(actual_csrcs, webrtc_metadata.GetCsrcs());
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorMissingFieldsFails) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());
  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameOptions* empty_frame_options =
      RTCEncodedVideoFrameOptions::Create();
  empty_frame_options->setMetadata(RTCEncodedVideoFrameMetadata::Create());

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, empty_frame_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame: new metadata has member(s) "
            "missing.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorWithoutFeatureFailsModifications) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(1));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(CreateMetadata(/*change_all_fields=*/true));

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame: invalid modification of "
            "RTCEncodedVideoFrameMetadata.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorWithFeatureAllowsModifications) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(14));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  RTCEncodedVideoFrameMetadata* new_metadata =
      CreateMetadata(/*change_all_fields=*/true);
  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();

  EXPECT_EQ(new_frame->getMetadata()->frameId(), new_metadata->frameId());
  Vector<int64_t> actual_dependencies;
  for (const auto& dependency : new_frame->getMetadata()->dependencies()) {
    actual_dependencies.push_back(dependency);
  }
  EXPECT_EQ(actual_dependencies, new_metadata->dependencies());
  EXPECT_EQ(new_frame->getMetadata()->width(), new_metadata->width());
  EXPECT_EQ(new_frame->getMetadata()->height(), new_metadata->height());
  EXPECT_EQ(new_frame->getMetadata()->spatialIndex(),
            new_metadata->spatialIndex());
  EXPECT_EQ(new_frame->getMetadata()->temporalIndex(),
            new_metadata->temporalIndex());
  EXPECT_EQ(new_frame->getMetadata()->synchronizationSource(),
            new_metadata->synchronizationSource());
  Vector<uint32_t> actual_csrcs;
  for (const auto& dependency :
       new_frame->getMetadata()->contributingSources()) {
    actual_csrcs.push_back(dependency);
  }
  EXPECT_EQ(actual_csrcs, new_metadata->contributingSources());
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorFromNull) {
  V8TestingScope v8_scope;
  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame =
      RTCEncodedVideoFrame::Create(nullptr, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame from an empty VideoFrame");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorOnEmptyFrameWorks) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  // Move the WebRTC frame out, as if the frame had been written into
  // an encoded insertable stream's WritableStream to be sent on.
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame =
      RTCEncodedVideoFrame::Create(encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_NE(new_frame, nullptr);
  EXPECT_EQ(new_frame->type(), "empty");
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorWithMetadataOnEmptyFrameFails) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(encoded_frame->getMetadata());
  // Move the WebRTC frame out, as if the frame had been written into
  // an encoded insertable stream's WritableStream to be sent on.
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame: underlying webrtc frame is "
            "an empty frame.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorRejectsInvalidDependencies) {
  V8TestingScope v8_scope;
  base::test::ScopedFeatureList feature_list_;
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAllowRTCEncodedVideoFrameSetMetadataAllFields},
      /*disabled_features=*/{});

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* new_metadata = CreateMetadata();
  // Set an invalid dependency - all deps must be less than frame id.
  new_metadata->setDependencies({new_metadata->frameId()});

  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame: new metadata has invalid "
            "frame dependencies.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorCopiesMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());
  EXPECT_CALL(*frame, GetTimestamp()).WillRepeatedly(Return(1));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame =
      RTCEncodedVideoFrame::Create(encoded_frame, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();

  EXPECT_EQ(new_frame->getMetadata()->frameId(),
            encoded_frame->getMetadata()->frameId());
  EXPECT_EQ(new_frame->getMetadata()->dependencies(),
            encoded_frame->getMetadata()->dependencies());
  EXPECT_EQ(new_frame->getMetadata()->width(),
            encoded_frame->getMetadata()->width());
  EXPECT_EQ(new_frame->getMetadata()->height(),
            encoded_frame->getMetadata()->height());
  EXPECT_EQ(new_frame->getMetadata()->spatialIndex(),
            encoded_frame->getMetadata()->spatialIndex());
  EXPECT_EQ(new_frame->getMetadata()->temporalIndex(),
            encoded_frame->getMetadata()->temporalIndex());
  EXPECT_EQ(new_frame->getMetadata()->synchronizationSource(),
            encoded_frame->getMetadata()->synchronizationSource());
  EXPECT_EQ(new_frame->getMetadata()->contributingSources(),
            encoded_frame->getMetadata()->contributingSources());
  EXPECT_EQ(new_frame->getMetadata()->rtpTimestamp(),
            encoded_frame->getMetadata()->rtpTimestamp());
}

TEST_F(RTCEncodedVideoFrameTest, ConstructorWithMetadataGetsNewMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(1));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameMetadata* new_metadata = CreateMetadata();
  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(new_metadata);

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);

  EXPECT_FALSE(exception_state.HadException()) << exception_state.Message();

  // |new_frame|'s metadata is same as |new_metadata|.
  EXPECT_EQ(new_frame->getMetadata()->frameId(), new_metadata->frameId());
  Vector<int64_t> actual_dependencies;
  for (const auto& dependency : new_frame->getMetadata()->dependencies()) {
    actual_dependencies.push_back(dependency);
  }
  EXPECT_EQ(actual_dependencies, new_metadata->dependencies());
  EXPECT_EQ(new_frame->getMetadata()->width(), new_metadata->width());
  EXPECT_EQ(new_frame->getMetadata()->height(), new_metadata->height());
  EXPECT_EQ(new_frame->getMetadata()->spatialIndex(),
            new_metadata->spatialIndex());
  EXPECT_EQ(new_frame->getMetadata()->temporalIndex(),
            new_metadata->temporalIndex());
  EXPECT_EQ(new_frame->getMetadata()->synchronizationSource(),
            new_metadata->synchronizationSource());
  Vector<uint32_t> actual_csrcs;
  for (const auto& dependency :
       new_frame->getMetadata()->contributingSources()) {
    actual_csrcs.push_back(dependency);
  }
  EXPECT_EQ(actual_csrcs, new_metadata->contributingSources());

  // |new_frame|'s metadata is different from original |encoded_frame|'s
  // metadata.
  EXPECT_NE(new_frame->getMetadata()->frameId(),
            encoded_frame->getMetadata()->frameId());
  EXPECT_NE(new_frame->getMetadata()->dependencies(),
            encoded_frame->getMetadata()->dependencies());
  EXPECT_NE(new_frame->getMetadata()->rtpTimestamp(),
            encoded_frame->getMetadata()->rtpTimestamp());
}

TEST_F(RTCEncodedVideoFrameTest,
       ConstructorWithMetadataDoesNotAllowChangingPayloadType) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();
  MockVP8Metadata(frame.get());

  webrtc::VideoFrameMetadata actual_metadata;
  EXPECT_CALL(*frame, SetMetadata(_)).Times(0);
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(14));

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  RTCEncodedVideoFrameOptions* frame_options =
      RTCEncodedVideoFrameOptions::Create();
  frame_options->setMetadata(CreateMetadata());

  DummyExceptionStateForTesting exception_state;
  RTCEncodedVideoFrame* new_frame = RTCEncodedVideoFrame::Create(
      encoded_frame, frame_options, exception_state);
  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.Message(),
            "Cannot create a new VideoFrame: invalid modification of "
            "payloadType in RTCEncodedVideoFrameMetadata.");
  EXPECT_EQ(new_frame, nullptr);
}

TEST_F(RTCEncodedVideoFrameTest, ReadingDataOnEmptyFrameGivesDetachedFrame) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/false);

  DOMArrayBuffer* data = encoded_frame->data(v8_scope.GetExecutionContext());
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(data->IsDetached());
}

TEST_F(RTCEncodedVideoFrameTest, PassWebRTCDetachesFrameData) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<NiceMock<MockTransformableVideoFrame>>();

  RTCEncodedVideoFrame* encoded_frame =
      MakeGarbageCollected<RTCEncodedVideoFrame>(std::move(frame));

  DOMArrayBuffer* data = encoded_frame->data(v8_scope.GetExecutionContext());
  encoded_frame->PassWebRtcFrame(v8_scope.GetIsolate(),
                                 /*detach_frame_data=*/true);
  EXPECT_NE(data, nullptr);
  EXPECT_TRUE(data->IsDetached());
}

}  // namespace blink

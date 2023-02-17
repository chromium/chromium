// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_codec_specifics_vp_8.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame_metadata.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame_delegate.h"
#include "third_party/webrtc/api/test/mock_transformable_video_frame.h"

using testing::Return;
using testing::ReturnRef;

using webrtc::MockTransformableVideoFrame;

namespace blink {

class RTCEncodedVideoFrameTest : public testing::Test {};

TEST_F(RTCEncodedVideoFrameTest, GetMetadataReturnsMetadata) {
  V8TestingScope v8_scope;

  std::unique_ptr<MockTransformableVideoFrame> frame =
      std::make_unique<MockTransformableVideoFrame>();

  webrtc::VideoFrameMetadata webrtc_metadata;
  webrtc_metadata.SetFrameId(1);
  webrtc_metadata.SetFrameDependencies(std::vector<int64_t>{2});
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

  EXPECT_CALL(*frame, GetMetadata()).WillOnce(ReturnRef(webrtc_metadata));
  EXPECT_CALL(*frame, GetPayloadType()).WillRepeatedly(Return(13));

  RTCEncodedVideoFrame encoded_frame(std::move(frame));

  RTCEncodedVideoFrameMetadata* retrieved_metadata =
      encoded_frame.getMetadata();
  EXPECT_EQ(7u, retrieved_metadata->synchronizationSource());
  EXPECT_EQ(13, retrieved_metadata->payloadType());
  EXPECT_EQ(1, retrieved_metadata->frameId());
  ASSERT_EQ(1u, retrieved_metadata->dependencies().size());
  EXPECT_EQ(2, retrieved_metadata->dependencies()[0]);
  EXPECT_EQ(800, retrieved_metadata->width());
  EXPECT_EQ(600, retrieved_metadata->height());
  EXPECT_EQ(3, retrieved_metadata->spatialIndex());
  EXPECT_EQ(4, retrieved_metadata->temporalIndex());
  ASSERT_EQ(1u, retrieved_metadata->decodeTargetIndications().size());
  EXPECT_EQ(V8RTCDecodeTargetIndication::Enum::kRequired,
            retrieved_metadata->decodeTargetIndications()[0]);
  EXPECT_EQ(true, retrieved_metadata->isLastFrameInPicture());
  EXPECT_EQ(5, retrieved_metadata->simulcastIdx());
  EXPECT_EQ(V8RTCVideoCodecType::Enum::kVp8, retrieved_metadata->codec());
  EXPECT_EQ(V8RTCEncodedVideoFrameType::Enum::kKey,
            retrieved_metadata->frameType());
  ASSERT_EQ(1u, retrieved_metadata->contributingSources().size());
  EXPECT_EQ(6u, retrieved_metadata->contributingSources()[0]);

  RTCCodecSpecificsVP8* retrieved_vp8_specifics =
      retrieved_metadata->codecSpecifics();

  EXPECT_EQ(true, retrieved_vp8_specifics->nonReference());
  EXPECT_EQ(8, retrieved_vp8_specifics->pictureId());
  EXPECT_EQ(9, retrieved_vp8_specifics->tl0PicIdx());
  EXPECT_EQ(10, retrieved_vp8_specifics->temporalIdx());
  EXPECT_EQ(true, retrieved_vp8_specifics->layerSync());
  EXPECT_EQ(11, retrieved_vp8_specifics->keyIdx());
  EXPECT_EQ(12, retrieved_vp8_specifics->partitionId());
  EXPECT_EQ(true, retrieved_vp8_specifics->beginningOfPartition());
}

}  // namespace blink

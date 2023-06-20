// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_AUDIO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_AUDIO_FRAME_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

class MockTransformableAudioFrame
    : public webrtc::TransformableAudioFrameInterface {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t>), (override));
  MOCK_METHOD(void, SetRTPTimestamp, (uint32_t), (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const override));
  MOCK_METHOD(const webrtc::RTPHeader&, GetHeader, (), (const override));
  MOCK_METHOD(rtc::ArrayView<const uint32_t>,
              GetContributingSources,
              (),
              (const override));
  MOCK_METHOD(webrtc::TransformableFrameInterface::Direction,
              GetDirection,
              (),
              (const, override));
  MOCK_METHOD(const absl::optional<uint16_t>,
              SequenceNumber,
              (),
              (const, override));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_AUDIO_FRAME_H_

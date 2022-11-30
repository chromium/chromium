// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_VIDEO_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_VIDEO_FRAME_H_

#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/array_view.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"
#include "third_party/webrtc/api/video/video_frame_metadata.h"

namespace blink {

class MockTransformableVideoFrame
    : public webrtc::TransformableVideoFrameInterface {
 public:
  MOCK_METHOD(rtc::ArrayView<const uint8_t>, GetData, (), (const override));
  MOCK_METHOD(void, SetData, (rtc::ArrayView<const uint8_t> data), (override));
  MOCK_METHOD(uint8_t, GetPayloadType, (), (const, override));
  MOCK_METHOD(uint32_t, GetSsrc, (), (const, override));
  MOCK_METHOD(uint32_t, GetTimestamp, (), (const override));
  MOCK_METHOD(bool, IsKeyFrame, (), (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetAdditionalData, (), (const, override));
  MOCK_METHOD(const webrtc::VideoFrameMetadata&,
              GetMetadata,
              (),
              (const, override));
  MOCK_METHOD(webrtc::TransformableFrameInterface::Direction,
              GetDirection,
              (),
              (const, override));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_TRANSFORMABLE_VIDEO_FRAME_H_

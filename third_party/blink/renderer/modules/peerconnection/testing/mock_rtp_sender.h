// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_SENDER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_SENDER_H_

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/rtp_parameters.h"
#include "third_party/webrtc/api/rtp_sender_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_count.h"

namespace blink {

class MockRtpSender : public rtc::RefCountedObject<webrtc::RtpSenderInterface> {
 public:
  MOCK_METHOD(bool, SetTrack, (webrtc::MediaStreamTrackInterface*), (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>,
              track,
              (),
              (const override));
  MOCK_METHOD(uint32_t, ssrc, (), (const override));
  MOCK_METHOD(cricket::MediaType, media_type, (), (const override));
  MOCK_METHOD(std::string, id, (), (const override));
  MOCK_METHOD(std::vector<std::string>, stream_ids, (), (const override));
  MOCK_METHOD(std::vector<webrtc::RtpEncodingParameters>,
              init_send_encodings,
              (),
              (const override));
  MOCK_METHOD(webrtc::RtpParameters, GetParameters, (), (const override));
  MOCK_METHOD(webrtc::RTCError,
              SetParameters,
              (const webrtc::RtpParameters&),
              (override));
  MOCK_METHOD(rtc::scoped_refptr<webrtc::DtmfSenderInterface>,
              GetDtmfSender,
              (),
              (const override));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_SENDER_H_

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_

#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/api/rtp_parameters.h"
#include "third_party/webrtc/api/rtp_receiver_interface.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/ref_count.h"

namespace blink {

class MockRtpReceiver
    : public rtc::RefCountedObject<webrtc::RtpReceiverInterface> {
 public:
  MOCK_METHOD(rtc::scoped_refptr<webrtc::MediaStreamTrackInterface>,
              track,
              (),
              (const override));
  MOCK_METHOD(std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>,
              streams,
              (),
              (const override));
  MOCK_METHOD(cricket::MediaType, media_type, (), (const override));
  MOCK_METHOD(std::string, id, (), (const override));
  MOCK_METHOD(webrtc::RtpParameters, GetParameters, (), (const override));
  MOCK_METHOD(void,
              SetObserver,
              (webrtc::RtpReceiverObserverInterface*),
              (override));
  MOCK_METHOD(void,
              SetJitterBufferMinimumDelay,
              (absl::optional<double>),
              (override));
  MOCK_METHOD(std::vector<webrtc::RtpSource>, GetSources, (), (const override));
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_

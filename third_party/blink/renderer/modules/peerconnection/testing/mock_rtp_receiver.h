// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_

#include "third_party/webrtc/api/test/mock_rtpreceiver.h"
#include "third_party/webrtc/rtc_base/ref_counted_object.h"

namespace blink {

class MockRtpReceiver
    : public webrtc::RefCountedObject<webrtc::MockRtpReceiver> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_TESTING_MOCK_RTP_RECEIVER_H_

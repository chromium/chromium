// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/mock_rtc_peer_connection_handler_client.h"

#include <memory>

#include "base/check_op.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_receiver_platform.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

using testing::_;

namespace blink {

MockRTCPeerConnectionHandlerClient::MockRTCPeerConnectionHandlerClient() {
  ON_CALL(*this, DidGenerateICECandidate(_))
      .WillByDefault(testing::Invoke(
          this,
          &MockRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker));
}

MockRTCPeerConnectionHandlerClient::~MockRTCPeerConnectionHandlerClient() {}

void MockRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker(
    RTCIceCandidatePlatform* candidate) {
  candidate_sdp_ = candidate->Candidate().Utf8();
  candidate_mline_index_ = candidate->SdpMLineIndex();
  candidate_mid_ = candidate->SdpMid().Utf8();
}

void MockRTCPeerConnectionHandlerClient::didModifyReceiversWorker(
    webrtc::PeerConnectionInterface::SignalingState signaling_state,
    Vector<std::unique_ptr<RTCRtpReceiverPlatform>>* receivers_added,
    Vector<std::unique_ptr<RTCRtpReceiverPlatform>>* receivers_removed) {
  // This fake implication is very limited. It is only used as a sanity check
  // if a stream was added or removed.
  if (!receivers_added->empty()) {
    Vector<String> stream_ids = (*receivers_added)[0]->StreamIds();
    DCHECK_EQ(1u, stream_ids.size());
    remote_stream_id_ = stream_ids[0];
  } else if (receivers_removed->empty()) {
    remote_stream_id_ = String();
  }
}

}  // namespace blink

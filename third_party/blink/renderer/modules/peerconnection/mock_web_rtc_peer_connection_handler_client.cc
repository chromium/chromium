// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/mock_web_rtc_peer_connection_handler_client.h"

#include "base/logging.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_string.h"

using testing::_;

namespace blink {

MockWebRTCPeerConnectionHandlerClient::MockWebRTCPeerConnectionHandlerClient() {
  ON_CALL(*this, DidGenerateICECandidate(_))
      .WillByDefault(
          testing::Invoke(this, &MockWebRTCPeerConnectionHandlerClient::
                                    didGenerateICECandidateWorker));
  ON_CALL(*this, DidAddReceiverPlanBForMock(_))
      .WillByDefault(testing::Invoke(
          this, &MockWebRTCPeerConnectionHandlerClient::didAddReceiverWorker));
  ON_CALL(*this, DidRemoveReceiverPlanBForMock(_))
      .WillByDefault(testing::Invoke(
          this,
          &MockWebRTCPeerConnectionHandlerClient::didRemoveReceiverWorker));
}

MockWebRTCPeerConnectionHandlerClient::
    ~MockWebRTCPeerConnectionHandlerClient() {}

void MockWebRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker(
    scoped_refptr<blink::WebRTCICECandidate> candidate) {
  candidate_sdp_ = candidate->Candidate().Utf8();
  candidate_mline_index_ = candidate->SdpMLineIndex();
  candidate_mid_ = candidate->SdpMid().Utf8();
}

void MockWebRTCPeerConnectionHandlerClient::didAddReceiverWorker(
    std::unique_ptr<blink::WebRTCRtpReceiver>* web_rtp_receiver) {
  blink::WebVector<blink::WebString> stream_ids =
      (*web_rtp_receiver)->StreamIds();
  DCHECK_EQ(1u, stream_ids.size());
  remote_stream_id_ = stream_ids[0];
}

void MockWebRTCPeerConnectionHandlerClient::didRemoveReceiverWorker(
    std::unique_ptr<blink::WebRTCRtpReceiver>* web_rtp_receiver) {
  remote_stream_id_ = blink::WebString();
}

}  // namespace blink

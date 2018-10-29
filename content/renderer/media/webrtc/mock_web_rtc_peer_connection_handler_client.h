// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/public/platform/web_rtc_rtp_receiver.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"

namespace content {

class MockWebRTCPeerConnectionHandlerClient
    : public blink::WebRTCPeerConnectionHandlerClient {
 public:
  MockWebRTCPeerConnectionHandlerClient();
  ~MockWebRTCPeerConnectionHandlerClient() override;

  // WebRTCPeerConnectionHandlerClient implementation.
  MOCK_METHOD0(NegotiationNeeded, void());
  MOCK_METHOD1(DidGenerateICECandidate,
               void(scoped_refptr<blink::WebRTCICECandidate> candidate));
  MOCK_METHOD1(DidChangeSignalingState,
               void(webrtc::PeerConnectionInterface::SignalingState state));
  MOCK_METHOD1(DidChangeIceGatheringState,
               void(webrtc::PeerConnectionInterface::IceGatheringState state));
  MOCK_METHOD1(DidChangeIceConnectionState,
               void(webrtc::PeerConnectionInterface::IceConnectionState state));
  void DidAddReceiverPlanB(
      std::unique_ptr<blink::WebRTCRtpReceiver> web_rtp_receiver) override {
    DidAddReceiverPlanBForMock(&web_rtp_receiver);
  }
  void DidRemoveReceiverPlanB(
      std::unique_ptr<blink::WebRTCRtpReceiver> web_rtp_receiver) override {
    DidRemoveReceiverPlanBForMock(&web_rtp_receiver);
  }
  void DidModifyTransceivers(
      std::vector<std::unique_ptr<blink::WebRTCRtpTransceiver>>
          web_transceivers,
      bool is_remote_description) override {
    DidModifyTransceiversForMock(&web_transceivers, is_remote_description);
  }
  MOCK_METHOD1(DidAddRemoteDataChannel, void(blink::WebRTCDataChannelHandler*));
  MOCK_METHOD1(DidNoteInterestingUsage, void(int));
  MOCK_METHOD0(ReleasePeerConnectionHandler, void());

  // Move-only arguments do not play nicely with MOCK, the workaround is to
  // EXPECT_CALL with these instead.
  MOCK_METHOD1(DidAddReceiverPlanBForMock,
               void(std::unique_ptr<blink::WebRTCRtpReceiver>*));
  MOCK_METHOD1(DidRemoveReceiverPlanBForMock,
               void(std::unique_ptr<blink::WebRTCRtpReceiver>*));
  MOCK_METHOD2(DidModifyTransceiversForMock,
               void(std::vector<std::unique_ptr<blink::WebRTCRtpTransceiver>>*,
                    bool));

  void didGenerateICECandidateWorker(
      scoped_refptr<blink::WebRTCICECandidate> candidate);
  void didAddReceiverWorker(
      std::unique_ptr<blink::WebRTCRtpReceiver>* stream_web_rtp_receivers);
  void didRemoveReceiverWorker(
      std::unique_ptr<blink::WebRTCRtpReceiver>* stream_web_rtp_receivers);

  const std::string& candidate_sdp() const { return candidate_sdp_; }
  int candidate_mlineindex() const {
    return candidate_mline_index_;
  }
  const std::string& candidate_mid() const { return candidate_mid_ ; }
  const blink::WebString& remote_stream_id() const { return remote_stream_id_; }

 private:
  blink::WebString remote_stream_id_;
  std::string candidate_sdp_;
  int candidate_mline_index_;
  std::string candidate_mid_;

  DISALLOW_COPY_AND_ASSIGN(MockWebRTCPeerConnectionHandlerClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_MOCK_WEB_RTC_PEER_CONNECTION_HANDLER_CLIENT_H_

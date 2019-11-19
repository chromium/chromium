// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_rtc_ice_candidate.h"

#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/p2p/base/p2p_constants.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

namespace blink {

namespace {

// Maps |component| to constants defined in
// https://w3c.github.io/webrtc-pc/#dom-rtcicecomponent
blink::WebString CandidateComponentToWebString(int component) {
  if (component == cricket::ICE_CANDIDATE_COMPONENT_RTP)
    return blink::WebString::FromASCII("rtp");
  if (component == cricket::ICE_CANDIDATE_COMPONENT_RTCP)
    return blink::WebString::FromASCII("rtcp");
  return blink::WebString();
}

// Maps |type| to constants defined in
// https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
blink::WebString CandidateTypeToWebString(const std::string& type) {
  if (type == cricket::LOCAL_PORT_TYPE)
    return blink::WebString::FromASCII("host");
  if (type == cricket::STUN_PORT_TYPE)
    return blink::WebString::FromASCII("srflx");
  if (type == cricket::PRFLX_PORT_TYPE)
    return blink::WebString::FromASCII("prflx");
  if (type == cricket::RELAY_PORT_TYPE)
    return blink::WebString::FromASCII("relay");
  return blink::WebString();
}

}  // namespace

// static
scoped_refptr<WebRTCICECandidate> WebRTCICECandidate::Create(
    WebString candidate,
    WebString sdp_mid,
    base::Optional<uint16_t> sdp_m_line_index,
    WebString username_fragment) {
  return base::AdoptRef(new WebRTCICECandidate(
      std::move(candidate), std::move(sdp_mid), std::move(sdp_m_line_index),
      std::move(username_fragment)));
}

scoped_refptr<WebRTCICECandidate> WebRTCICECandidate::Create(
    WebString candidate,
    WebString sdp_mid,
    int sdp_m_line_index) {
  return base::AdoptRef(new WebRTCICECandidate(
      std::move(candidate), std::move(sdp_mid),
      sdp_m_line_index < 0 ? base::Optional<uint16_t>()
                           : base::Optional<uint16_t>(sdp_m_line_index)));
}

WebRTCICECandidate::WebRTCICECandidate(
    WebString candidate,
    WebString sdp_mid,
    base::Optional<uint16_t> sdp_m_line_index,
    WebString username_fragment)
    : candidate_(std::move(candidate)),
      sdp_mid_(std::move(sdp_mid)),
      sdp_m_line_index_(std::move(sdp_m_line_index)),
      username_fragment_(std::move(username_fragment)) {
  PopulateFields(false);
}

WebRTCICECandidate::WebRTCICECandidate(
    WebString candidate,
    WebString sdp_mid,
    base::Optional<uint16_t> sdp_m_line_index)
    : candidate_(std::move(candidate)),
      sdp_mid_(std::move(sdp_mid)),
      sdp_m_line_index_(std::move(sdp_m_line_index)) {
  PopulateFields(true);
}

void WebRTCICECandidate::PopulateFields(bool use_username_from_candidate) {
  cricket::Candidate c;
  if (!webrtc::ParseCandidate(candidate_.Utf8(), &c, nullptr, true))
    return;

  foundation_ = blink::WebString::FromUTF8(c.foundation());
  component_ = CandidateComponentToWebString(c.component());
  priority_ = c.priority();
  protocol_ = blink::WebString::FromUTF8(c.protocol());
  if (!c.address().IsNil()) {
    address_ = blink::WebString::FromUTF8(c.address().HostAsURIString());
    port_ = c.address().port();
  }
  type_ = CandidateTypeToWebString(c.type());
  tcp_type_ = blink::WebString::FromUTF8(c.tcptype());
  if (!c.related_address().IsNil()) {
    related_address_ =
        blink::WebString::FromUTF8(c.related_address().HostAsURIString());
    related_port_ = c.related_address().port();
  }

  if (use_username_from_candidate)
    username_fragment_ = blink::WebString::FromUTF8(c.username());
}

}  // namespace blink

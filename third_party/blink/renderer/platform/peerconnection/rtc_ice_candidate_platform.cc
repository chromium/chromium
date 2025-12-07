// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"

#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/p2p/base/p2p_constants.h"

namespace blink {

namespace {

// Maps |component| to constants defined in
// https://w3c.github.io/webrtc-pc/#dom-rtcicecomponent
String CandidateComponentToString(int component) {
  if (component == webrtc::ICE_CANDIDATE_COMPONENT_RTP) {
    return String("rtp");
  }
  if (component == webrtc::ICE_CANDIDATE_COMPONENT_RTCP) {
    return String("rtcp");
  }
  return String();
}

// Determine the relay protocol from local type preference which is the
// lower 8 bits of the priority. The mapping to relay protocol is defined
// in webrtc/p2p/base/port.h and only valid for relay candidates.
String PriorityToRelayProtocol(uint32_t priority) {
  uint8_t local_type_preference = priority >> 24;
  switch (local_type_preference) {
    case 0:
      return String("tls");
    case 1:
      return String("tcp");
    case 2:
      return String("udp");
  }
  return String();
}

}  // namespace

RTCIceCandidatePlatform::RTCIceCandidatePlatform(
    String candidate,
    String sdp_mid,
    std::optional<uint16_t> sdp_m_line_index,
    String username_fragment,
    String url)
    : candidate_(std::move(candidate)),
      sdp_mid_(std::move(sdp_mid)),
      sdp_m_line_index_(std::move(sdp_m_line_index)),
      username_fragment_(std::move(username_fragment)),
      url_(std::move(url)) {
  PopulateFields();
}

void RTCIceCandidatePlatform::PopulateFields() {
  webrtc::RTCErrorOr<webrtc::Candidate> parsed_candidate =
      webrtc::Candidate::ParseCandidateString(candidate_.Utf8());
  if (!parsed_candidate.ok()) {
    return;
  }

  const webrtc::Candidate& c = parsed_candidate.value();

  foundation_ = String::FromUTF8(c.foundation());
  component_ = CandidateComponentToString(c.component());
  priority_ = c.priority();
  protocol_ = String::FromUTF8(c.protocol());
  if (!c.address().IsNil()) {
    address_ = String::FromUTF8(c.address().HostAsURIString());
    port_ = c.address().port();
  }
  // The `type_name()` property returns a name as specified in:
  // https://datatracker.ietf.org/doc/html/rfc5245#section-15.1
  // which is identical to:
  // https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
  auto type = c.type_name();
  DCHECK(type == "host" || type == "srflx" || type == "prflx" ||
         type == "relay");
  type_ = String(type);
  if (!c.tcptype().empty()) {
    tcp_type_ = String::FromUTF8(c.tcptype());
  }
  if (!c.related_address().IsNil()) {
    related_address_ = String::FromUTF8(c.related_address().HostAsURIString());
    related_port_ = c.related_address().port();
  }
  // url_ is set only when the candidate was gathered locally.
  if (type_ == "relay" && priority_ && !url_.IsNull()) {
    relay_protocol_ = PriorityToRelayProtocol(*priority_);
  }
}

}  // namespace blink

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_ice_candidate_platform.h"

#include "third_party/webrtc/api/candidate.h"
#include "third_party/webrtc/p2p/base/p2p_constants.h"
#include "third_party/webrtc/p2p/base/port.h"
#include "third_party/webrtc/pc/webrtc_sdp.h"

namespace blink {

namespace {

// Maps |component| to constants defined in
// https://w3c.github.io/webrtc-pc/#dom-rtcicecomponent
String CandidateComponentToString(int component) {
  if (component == cricket::ICE_CANDIDATE_COMPONENT_RTP)
    return String("rtp");
  if (component == cricket::ICE_CANDIDATE_COMPONENT_RTCP)
    return String("rtcp");
  return String();
}

// Maps |type| to constants defined in
// https://w3c.github.io/webrtc-pc/#rtcicecandidatetype-enum
String CandidateTypeToString(const std::string& type) {
  if (type == cricket::LOCAL_PORT_TYPE)
    return String("host");
  if (type == cricket::STUN_PORT_TYPE)
    return String("srflx");
  if (type == cricket::PRFLX_PORT_TYPE)
    return String("prflx");
  if (type == cricket::RELAY_PORT_TYPE)
    return String("relay");
  return String();
}

}  // namespace

RTCIceCandidatePlatform::RTCIceCandidatePlatform(
    String candidate,
    String sdp_mid,
    absl::optional<uint16_t> sdp_m_line_index,
    String username_fragment)
    : candidate_(std::move(candidate)),
      sdp_mid_(std::move(sdp_mid)),
      sdp_m_line_index_(std::move(sdp_m_line_index)),
      username_fragment_(std::move(username_fragment)) {
  PopulateFields(false);
}

RTCIceCandidatePlatform::RTCIceCandidatePlatform(
    String candidate,
    String sdp_mid,
    absl::optional<uint16_t> sdp_m_line_index)
    : candidate_(std::move(candidate)),
      sdp_mid_(std::move(sdp_mid)),
      sdp_m_line_index_(std::move(sdp_m_line_index)) {
  PopulateFields(true);
}

void RTCIceCandidatePlatform::PopulateFields(bool use_username_from_candidate) {
  cricket::Candidate c;
  if (!webrtc::ParseCandidate(candidate_.Utf8(), &c, nullptr, true))
    return;

  foundation_ = String::FromUTF8(c.foundation().data());
  component_ = CandidateComponentToString(c.component());
  priority_ = c.priority();
  protocol_ = String::FromUTF8(c.protocol().data());
  if (!c.address().IsNil()) {
    address_ = String::FromUTF8(c.address().HostAsURIString().data());
    port_ = c.address().port();
  }
  type_ = CandidateTypeToString(c.type());
  if (!c.tcptype().empty()) {
    tcp_type_ = String::FromUTF8(c.tcptype().data());
  }
  if (!c.related_address().IsNil()) {
    related_address_ =
        String::FromUTF8(c.related_address().HostAsURIString().data());
    related_port_ = c.related_address().port();
  }

  if (use_username_from_candidate)
    username_fragment_ = String::FromUTF8(c.username().data());
}

}  // namespace blink

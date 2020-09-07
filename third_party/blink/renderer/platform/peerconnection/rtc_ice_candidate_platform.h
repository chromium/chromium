/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ICE_CANDIDATE_PLATFORM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ICE_CANDIDATE_PLATFORM_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PLATFORM_EXPORT RTCIceCandidatePlatform final
    : public GarbageCollected<RTCIceCandidatePlatform> {
 public:
  // Creates a new RTCIceCandidatePlatform using |candidate|, |sdp_mid| and
  // |sdp_m_line_index|. If |sdp_m_line_index| is negative, it is
  // considered as having no value.
  RTCIceCandidatePlatform(String candidate,
                          String sdp_mid,
                          base::Optional<uint16_t> sdp_m_line_index);

  // Creates a new RTCIceCandidatePlatform using |candidate|, |sdp_mid|,
  // |sdp_m_line_index|, and |username_fragment|.
  RTCIceCandidatePlatform(String candidate,
                          String sdp_mid,
                          base::Optional<uint16_t> sdp_m_line_index,
                          String username_fragment);
  ~RTCIceCandidatePlatform() = default;

  const String& Candidate() const { return candidate_; }
  const String& SdpMid() const { return sdp_mid_; }
  const base::Optional<uint16_t>& SdpMLineIndex() const {
    return sdp_m_line_index_;
  }
  const String& Foundation() const { return foundation_; }
  const String& Component() const { return component_; }
  const base::Optional<uint32_t>& Priority() const { return priority_; }
  const String& Address() const { return address_; }
  const String Protocol() const { return protocol_; }
  const base::Optional<uint16_t>& Port() const { return port_; }
  const String& Type() const { return type_; }
  const base::Optional<String>& TcpType() const { return tcp_type_; }
  const String& RelatedAddress() const { return related_address_; }
  const base::Optional<uint16_t>& RelatedPort() const { return related_port_; }
  const String& UsernameFragment() const { return username_fragment_; }

  void Trace(Visitor*) const {}

 private:
  void PopulateFields(bool use_username_from_candidate);

  String candidate_;
  String sdp_mid_;
  base::Optional<uint16_t> sdp_m_line_index_;
  String foundation_;
  String component_;
  base::Optional<uint32_t> priority_;
  String address_;
  String protocol_;
  base::Optional<uint16_t> port_;
  String type_;
  base::Optional<String> tcp_type_;
  String related_address_;
  base::Optional<uint16_t> related_port_;
  String username_fragment_;

  DISALLOW_COPY_AND_ASSIGN(RTCIceCandidatePlatform);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_RTC_ICE_CANDIDATE_PLATFORM_H_

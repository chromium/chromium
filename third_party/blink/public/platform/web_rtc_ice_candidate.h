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

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_ICE_CANDIDATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_ICE_CANDIDATE_H_

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebRTCICECandidate final
    : public base::RefCountedThreadSafe<WebRTCICECandidate> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  // Creates a new WebRTCICECandidate using |candidate|, |sdp_mid| and
  // |sdp_m_line_index|. If |sdp_m_line_index| is negative, it is
  // considered as having no value.
  static scoped_refptr<WebRTCICECandidate> Create(WebString candidate,
                                                  WebString sdp_mid,
                                                  int sdp_m_line_index);

  // Creates a new WebRTCICECandidate using |candidate|, |sdp_mid|,
  // |sdp_m_line_index|, and |username_fragment|.
  static scoped_refptr<WebRTCICECandidate> Create(
      WebString candidate,
      WebString sdp_mid,
      base::Optional<uint16_t> sdp_m_line_index,
      WebString username_fragment);

  const WebString& Candidate() const { return candidate_; }
  const WebString& SdpMid() const { return sdp_mid_; }
  const base::Optional<uint16_t>& SdpMLineIndex() const {
    return sdp_m_line_index_;
  }
  const WebString& Foundation() const { return foundation_; }
  const WebString& Component() const { return component_; }
  const base::Optional<uint32_t>& Priority() const { return priority_; }
  const WebString& Address() const { return address_; }
  const WebString Protocol() const { return protocol_; }
  const base::Optional<uint16_t>& Port() const { return port_; }
  const WebString& Type() const { return type_; }
  const WebString& TcpType() const { return tcp_type_; }
  const WebString& RelatedAddress() const { return related_address_; }
  const base::Optional<uint16_t>& RelatedPort() const { return related_port_; }
  const WebString& UsernameFragment() const { return username_fragment_; }

 private:
  friend class base::RefCountedThreadSafe<WebRTCICECandidate>;

  WebRTCICECandidate(WebString candidate,
                     WebString sdp_mid,
                     base::Optional<uint16_t> sdp_m_line_index);

  WebRTCICECandidate(WebString candidate,
                     WebString sdp_mid,
                     base::Optional<uint16_t> sdp_m_line_index,
                     WebString username_fragment);

  void PopulateFields(bool use_username_from_candidate);

  ~WebRTCICECandidate() = default;

  WebString candidate_;
  WebString sdp_mid_;
  base::Optional<uint16_t> sdp_m_line_index_;
  WebString foundation_;
  WebString component_;
  base::Optional<uint32_t> priority_;
  WebString address_;
  WebString protocol_;
  base::Optional<uint16_t> port_;
  WebString type_;
  WebString tcp_type_;
  WebString related_address_;
  base::Optional<uint16_t> related_port_;
  WebString username_fragment_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCICECandidate);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_RTC_ICE_CANDIDATE_H_

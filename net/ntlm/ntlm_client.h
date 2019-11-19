// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on [MS-NLMP]: NT LAN Manager (NTLM) Authentication Protocol
// Specification version 28.0 [1], an unofficial NTLM reference [2], and a
// blog post describing Extended Protection for Authentication [3].
//
// [1] https://msdn.microsoft.com/en-us/library/cc236621.aspx
// [2] http://davenport.sourceforge.net/ntlm.html
// [3]
// https://blogs.msdn.microsoft.com/openspecification/2013/03/26/ntlm-and-channel-binding-hash-aka-extended-protection-for-authentication/

#ifndef NET_BASE_NTLM_CLIENT_H_
#define NET_BASE_NTLM_CLIENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "net/base/net_export.h"
#include "net/ntlm/ntlm_constants.h"

namespace net {
namespace ntlm {

// Provides an implementation of an NTLMv1 or NTLMv2 Client with support
// for MIC and EPA [1]. This implementation does not support the key exchange,
// signing or sealing feature as the NTLMSSP_NEGOTIATE_KEY_EXCH flag is never
// negotiated.
//
// [1] -
// https://support.microsoft.com/en-us/help/968389/extended-protection-for-authentication
class NET_EXPORT_PRIVATE NtlmClient {
 public:
  // Pass feature flags to enable/disable NTLMv2 and additional NTLMv2
  // features such as Extended Protection for Authentication (EPA) and Message
  // Integrity Check (MIC).
  explicit NtlmClient(NtlmFeatures features);

  ~NtlmClient();

  bool IsNtlmV2() const { return features_.enable_NTLMv2; }

  bool IsMicEnabled() const { return IsNtlmV2() && features_.enable_MIC; }

  bool IsEpaEnabled() const { return IsNtlmV2() && features_.enable_EPA; }

  // Returns the Negotiate message.
  std::vector<uint8_t> GetNegotiateMessage() const;

  // Returns a the Authenticate message. If the method fails an empty vector
  // is returned.
  //
  // |username| is treated case insensitively by NTLM however the mechanism
  // to uppercase is not clearly defined. In this implementation the default
  // locale is used. Additionally for names longer than 20 characters, the
  // fully qualified name in the new '@' format must be used.
  // eg. very_long_name@domain.com. Names shorter than 20 characters can
  // optionally omit the '@domain.com' part.
  // |hostname| can be a short NetBIOS name or an FQDN, however the server will
  // only inspect this field if the default domain policy is to restrict NTLM.
  // In this case the hostname will be compared to an allowlist stored in this
  // group policy [1].
  // |channel_bindings| is a string supplied out of band (usually from a web
  // browser) and is a (21+sizeof(hash)) byte ASCII string, where 'hash' is
  // usually a SHA-256 of the servers certificate, but may be another hash
  // algorithm. The format as defined by RFC 5929 Section 4 is shown below;
  //
  // [0-20]                 - "tls-server-end-point:"   (Literal string)
  // [21-(20+sizeof(hash)]  - HASH(server_certificate)  (Certificate hash)
  //
  // |spn| is a string supplied out of band (usually from a web browser) and
  // is a Service  Principal Name [2]. For NTLM over HTTP the value of this
  // string will usually be "HTTP/<hostname>".
  // |client_time| 64 bit Windows timestamp defined as the number of
  // 100 nanosecond ticks since midnight Jan 01, 1601 (UTC). If the server does
  // not send a timestamp, the client timestamp is used in the Proof Input
  // instead.
  // |server_challenge_message| is the full content of the challenge message
  // sent by the server.
  //
  // [1] - https://technet.microsoft.com/en-us/library/jj852267(v=ws.11).aspx
  std::vector<uint8_t> GenerateAuthenticateMessage(
      const base::string16& domain,
      const base::string16& username,
      const base::string16& password,
      const std::string& hostname,
      const std::string& channel_bindings,
      const std::string& spn,
      uint64_t client_time,
      base::span<const uint8_t, kChallengeLen> client_challenge,
      base::span<const uint8_t> server_challenge_message) const;

  // Simplified method for NTLMv1 which does not require |channel_bindings|,
  // |spn|, or |client_time|. See |GenerateAuthenticateMessage| for more
  // details.
  std::vector<uint8_t> GenerateAuthenticateMessageV1(
      const base::string16& domain,
      const base::string16& username,
      const base::string16& password,
      const std::string& hostname,
      base::span<const uint8_t, 8> client_challenge,
      base::span<const uint8_t> server_challenge_message) const {
    DCHECK(!IsNtlmV2());

    return GenerateAuthenticateMessage(
        domain, username, password, hostname, std::string(), std::string(), 0,
        client_challenge, server_challenge_message);
  }

 private:
  // Returns the length of the Authenticate message based on the length of the
  // variable length parts of the message and whether Unicode support was
  // negotiated.
  size_t CalculateAuthenticateMessageLength(
      bool is_unicode,
      const base::string16& domain,
      const base::string16& username,
      const std::string& hostname,
      size_t updated_target_info_len) const;

  void CalculatePayloadLayout(bool is_unicode,
                              const base::string16& domain,
                              const base::string16& username,
                              const std::string& hostname,
                              size_t updated_target_info_len,
                              SecurityBuffer* lm_info,
                              SecurityBuffer* ntlm_info,
                              SecurityBuffer* domain_info,
                              SecurityBuffer* username_info,
                              SecurityBuffer* hostname_info,
                              SecurityBuffer* session_key_info,
                              size_t* authenticate_message_len) const;

  // Returns the length of the header part of the Authenticate message.
  size_t GetAuthenticateHeaderLength() const;

  // Returns the length of the NTLM response.
  size_t GetNtlmResponseLength(size_t updated_target_info_len) const;

  // Generates the negotiate message (which is always the same) into
  // |negotiate_message_|.
  void GenerateNegotiateMessage();

  const NtlmFeatures features_;
  NegotiateFlags negotiate_flags_;
  std::vector<uint8_t> negotiate_message_;

  DISALLOW_COPY_AND_ASSIGN(NtlmClient);
};

}  // namespace ntlm
}  // namespace net

#endif  // NET_BASE_NTLM_CLIENT_H_

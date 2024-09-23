// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Based on [MS-NLMP]: NT LAN Manager (NTLM) Authentication Protocol
// Specification version 28.0 [1]. Additional NTLM reference [2].
//
// [1] https://msdn.microsoft.com/en-us/library/cc236621.aspx
// [2] http://davenport.sourceforge.net/ntlm.html

#ifndef NET_NTLM_NTLM_H_
#define NET_NTLM_NTLM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "net/base/net_export.h"
#include "net/ntlm/ntlm_constants.h"

namespace net::ntlm {

// Maps the bits in the NTLM Hash into 3 DES keys. The DES keys each have 56
// bits stored in the 7 most significant bits of 8 bytes. The least
// significant bit is undefined and will subsequently be set with odd parity
// prior to use.
NET_EXPORT_PRIVATE void Create3DesKeysFromNtlmHash(
    base::span<const uint8_t, kNtlmHashLen> ntlm_hash,
    base::span<uint8_t, 24> keys);

// Generates the NTLMv1 Hash and writes the |kNtlmHashLen| byte result to
// |hash|. Defined by NTOWFv1() in [MS-NLMP] Section 3.3.1.
NET_EXPORT_PRIVATE void GenerateNtlmHashV1(
    const std::u16string& password,
    base::span<uint8_t, kNtlmHashLen> hash);

// Generates the |kResponseLenV1| byte NTLMv1 response field according to the
// DESL(K, V) function in [MS-NLMP] Section 6.
NET_EXPORT_PRIVATE void GenerateResponseDesl(
    base::span<const uint8_t, kNtlmHashLen> hash,
    base::span<const uint8_t, kChallengeLen> challenge,
    base::span<uint8_t, kResponseLenV1> response);

// Generates the NTLM Response field for NTLMv1 without extended session
// security. Defined by ComputeResponse() in [MS-NLMP] Section 3.3.1 for the
// case where NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY is not set.
NET_EXPORT_PRIVATE void GenerateNtlmResponseV1(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<uint8_t, kResponseLenV1> ntlm_response);

// Generates both the LM Response and NTLM Response fields for NTLMv1 based
// on the users password and the servers challenge. Both the LM and NTLM
// Response are the result of |GenerateNtlmResponseV1|.
//
// NOTE: This should not be used. The default flags always include session
// security. Session security can however be disabled in NTLMv1 by omitting
// NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY from the flag set used to
// initialize |NtlmClient|.
//
// The default flags include this flag and the client will not be
// downgraded by the server.
NET_EXPORT_PRIVATE void GenerateResponsesV1(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response,
    base::span<uint8_t, kResponseLenV1> ntlm_response);

// The LM Response in V1 with extended session security is 8 bytes of the
// |client_challenge| then 16 bytes of zero. This is the value
// LmChallengeResponse in ComputeResponse() when
// NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY is set. See [MS-NLMP] Section
// 3.3.1.
NET_EXPORT_PRIVATE void GenerateLMResponseV1WithSessionSecurity(
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response);

// The |session_hash| is MD5(CONCAT(server_challenge, client_challenge)).
// It is used instead of just |server_challenge| in NTLMv1 when
// NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY is set. See [MS-NLMP] Section
// 3.3.1.
NET_EXPORT_PRIVATE void GenerateSessionHashV1WithSessionSecurity(
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kNtlmHashLen> session_hash);

// Generates the NTLM Response for NTLMv1 with session security.
// Defined by ComputeResponse() in [MS-NLMP] Section 3.3.1 for the
// case where NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY is set.
NET_EXPORT_PRIVATE void GenerateNtlmResponseV1WithSessionSecurity(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> ntlm_response);

// Generates the responses for V1 with extended session security.
// This is also known as NTLM2 (which is not the same as NTLMv2).
// |lm_response| is the result of |GenerateLMResponseV1WithSessionSecurity| and
// |ntlm_response| is the result of |GenerateNtlmResponseV1WithSessionSecurity|.
// See [MS-NLMP] Section 3.3.1.
NET_EXPORT_PRIVATE void GenerateResponsesV1WithSessionSecurity(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response,
    base::span<uint8_t, kResponseLenV1> ntlm_response);

// Generates the NTLMv2 Hash and writes it into |v2_hash|.
NET_EXPORT_PRIVATE void GenerateNtlmHashV2(
    const std::u16string& domain,
    const std::u16string& username,
    const std::u16string& password,
    base::span<uint8_t, kNtlmHashLen> v2_hash);

// In this implementation the Proof Input is the first 28 bytes of what
// [MS-NLMP] section 3.3.2 calls "temp". "temp" is part of the input to
// generate the NTLMv2 proof. "temp" is composed of a fixed 28 byte prefix
// (the Proof Input), then the variable length updated target info that is
// sent in the authenticate message, then followed by 4 zero bytes. See
// [MS-NLMP] Section 2.2.2.7.
//
// |timestamp| contains a 64 bit Windows timestamp defined as the number of
// 100 nanosecond ticks since midnight Jan 01, 1601 (UTC).
//
// The format of the returned |proof_input| is;
//
// [0-1]    - 0x0101                              (Version)
// [2-7]    - 0x000000000000                      (Reserved - all zero)
// [8-15]   - |timestamp|                         (Timestamp)
// [16-23]  - |client_challenge|                  (Client challenge)
// [24-27]  - 0x00000000                          (Reserved - all zero)
NET_EXPORT_PRIVATE std::vector<uint8_t> GenerateProofInputV2(
    uint64_t timestamp,
    base::span<const uint8_t, kChallengeLen> client_challenge);

// The NTLMv2 Proof is part of the NTLMv2 Response. See NTProofStr in [MS-NLMP]
// Section 3.3.2.
//
// The NTLMv2 Proof is defined as;
//     v2_proof = HMAC_MD5(
//         v2_hash,
//         CONCAT(server_challenge, v2_input, target_info, 0x00000000))
NET_EXPORT_PRIVATE void GenerateNtlmProofV2(
    base::span<const uint8_t, kNtlmHashLen> v2_hash,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kProofInputLenV2> v2_input,
    base::span<const uint8_t> target_info,
    base::span<uint8_t, kNtlmProofLenV2> v2_proof);

// The session base key is used to generate the Message Integrity Check (MIC).
// See [MS-NLMP] Section 3.3.2.
//
// It is defined as;
//     session_key = HMAC_MD5(v2_hash, v2_proof)
NET_EXPORT_PRIVATE void GenerateSessionBaseKeyV2(
    base::span<const uint8_t, kNtlmHashLen> v2_hash,
    base::span<const uint8_t, kNtlmProofLenV2> v2_proof,
    base::span<uint8_t, kSessionKeyLenV2> session_key);

// The channel bindings hash is an MD5 hash of a data structure containing
// a hash of the server's certificate.
//
// The |channel_bindings| string is supplied out of band (usually from a web
// browser) and is a (21+sizeof(hash)) byte ASCII string, where 'hash' is
// usually a SHA-256 of the servers certificate, but may be another hash
// algorithm. The format as defined by RFC 5929 Section 4 is shown below;
//
// [0-20]                 - "tls-server-end-point:"   (Literal string)
// [21-(20+sizeof(hash)]  - HASH(server_certificate)  (Certificate hash)
//
// The |channel_bindings| string is then combined into a data structure called
// gss_channel_bindings_struct (on Windows SEC_CHANNEL_BINDINGS) and MD5 hashed
// according to the rules in RFC 4121 Section 4.1.1.2. When simplified this
// results in the input to the hash (aka "ClientChannelBindingsUnhashed")
// being defined as follows;
//
// [0-15]   - 16 zero bytes                        (Collapsed fields)
// [16-19]  - |strlen(channel_bindings)|           (Length=0x00000035)
// [20-72]  - |channel_bindings|                   (Channel bindings)
//
// See also RFC 5056 and [MS-NLMP] Section 3.1.5.1.2.
//
// The channel bindings hash is then defined as;
//     channel_bindings_hash = MD5(ClientChannelBindingsUnhashed)
NET_EXPORT_PRIVATE void GenerateChannelBindingHashV2(
    const std::string& channel_bindings,
    base::span<uint8_t, kNtlmHashLen> channel_bindings_hash);

// The Message Integrity Check (MIC) is a hash calculated over all three
// messages in the NTLM protocol. The MIC field in the authenticate message
// is set to all zeros when calculating the hash. See [MS-NLMP] Section
// 3.1.5.1.2.
//
// In this implementation NTLMSSP_NEGOTIATE_KEY_EXCH never negotiated and
// the MIC for this case is defined as below. If NTLMSSP_NEGOTIATE_KEY_EXCH
// was negotiated, an alternate key is used. See [MS-NLMP] SEction 3.1.5.1.2
// for additional details.
//
//     mic = HMAC_MD5(
//         session_base_key,
//         CONCAT(negotiate_msg, challenge_msg, authenticate_msg))
//
// |session_key| must contain |kSessionKeyLenV2| bytes.
// |mic| must contain |kMicLenV2| bytes.
NET_EXPORT_PRIVATE void GenerateMicV2(
    base::span<const uint8_t, kSessionKeyLenV2> session_key,
    base::span<const uint8_t> negotiate_msg,
    base::span<const uint8_t> challenge_msg,
    base::span<const uint8_t> authenticate_msg,
    base::span<uint8_t, kMicLenV2> mic);

// Updates the target info sent by the server, and generates the clients
// response target info.
NET_EXPORT_PRIVATE std::vector<uint8_t> GenerateUpdatedTargetInfo(
    bool is_mic_enabled,
    bool is_epa_enabled,
    const std::string& channel_bindings,
    const std::string& spn,
    const std::vector<AvPair>& av_pairs,
    uint64_t* server_timestamp);

}  // namespace net::ntlm

#endif  // NET_NTLM_NTLM_H_

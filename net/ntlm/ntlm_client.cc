// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm_client.h"

#include <string.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/utf_string_conversions.h"
#include "net/ntlm/ntlm.h"
#include "net/ntlm/ntlm_buffer_reader.h"
#include "net/ntlm/ntlm_buffer_writer.h"
#include "net/ntlm/ntlm_constants.h"

namespace net::ntlm {

namespace {
// Parses the challenge message and returns the |challenge_flags| and
// |server_challenge| into the supplied buffer.
bool ParseChallengeMessage(
    base::span<const uint8_t> challenge_message,
    NegotiateFlags* challenge_flags,
    base::span<uint8_t, kChallengeLen> server_challenge) {
  NtlmBufferReader challenge_reader(challenge_message);

  return challenge_reader.MatchMessageHeader(MessageType::kChallenge) &&
         challenge_reader.SkipSecurityBufferWithValidation() &&
         challenge_reader.ReadFlags(challenge_flags) &&
         challenge_reader.ReadBytes(server_challenge);
}

// Parses the challenge message and extracts the information necessary to
// make an NTLMv2 response.
bool ParseChallengeMessageV2(
    base::span<const uint8_t> challenge_message,
    NegotiateFlags* challenge_flags,
    base::span<uint8_t, kChallengeLen> server_challenge,
    std::vector<AvPair>* av_pairs) {
  NtlmBufferReader challenge_reader(challenge_message);

  return challenge_reader.MatchMessageHeader(MessageType::kChallenge) &&
         challenge_reader.SkipSecurityBufferWithValidation() &&
         challenge_reader.ReadFlags(challenge_flags) &&
         challenge_reader.ReadBytes(server_challenge) &&
         challenge_reader.SkipBytes(8) &&
         // challenge_reader.ReadTargetInfoPayload(av_pairs);
         (((*challenge_flags & NegotiateFlags::kTargetInfo) ==
           NegotiateFlags::kTargetInfo)
              ? challenge_reader.ReadTargetInfoPayload(av_pairs)
              : true);
}

bool WriteAuthenticateMessage(NtlmBufferWriter* authenticate_writer,
                              SecurityBuffer lm_payload,
                              SecurityBuffer ntlm_payload,
                              SecurityBuffer domain_payload,
                              SecurityBuffer username_payload,
                              SecurityBuffer hostname_payload,
                              SecurityBuffer session_key_payload,
                              NegotiateFlags authenticate_flags) {
  return authenticate_writer->WriteMessageHeader(MessageType::kAuthenticate) &&
         authenticate_writer->WriteSecurityBuffer(lm_payload) &&
         authenticate_writer->WriteSecurityBuffer(ntlm_payload) &&
         authenticate_writer->WriteSecurityBuffer(domain_payload) &&
         authenticate_writer->WriteSecurityBuffer(username_payload) &&
         authenticate_writer->WriteSecurityBuffer(hostname_payload) &&
         authenticate_writer->WriteSecurityBuffer(session_key_payload) &&
         authenticate_writer->WriteFlags(authenticate_flags);
}

// Writes the NTLMv1 LM Response and NTLM Response.
bool WriteResponsePayloads(
    NtlmBufferWriter* authenticate_writer,
    base::span<const uint8_t, kResponseLenV1> lm_response,
    base::span<const uint8_t, kResponseLenV1> ntlm_response) {
  return authenticate_writer->WriteBytes(lm_response) &&
         authenticate_writer->WriteBytes(ntlm_response);
}

// Writes the |lm_response| and writes the NTLMv2 response by concatenating
// |v2_proof|, |v2_proof_input|, |updated_target_info| and 4 zero bytes.
bool WriteResponsePayloadsV2(
    NtlmBufferWriter* authenticate_writer,
    base::span<const uint8_t, kResponseLenV1> lm_response,
    base::span<const uint8_t, kNtlmProofLenV2> v2_proof,
    base::span<const uint8_t> v2_proof_input,
    base::span<const uint8_t> updated_target_info) {
  return authenticate_writer->WriteBytes(lm_response) &&
         authenticate_writer->WriteBytes(v2_proof) &&
         authenticate_writer->WriteBytes(v2_proof_input) &&
         authenticate_writer->WriteBytes(updated_target_info) &&
         authenticate_writer->WriteUInt32(0);
}

bool WriteStringPayloads(NtlmBufferWriter* authenticate_writer,
                         bool is_unicode,
                         const std::u16string& domain,
                         const std::u16string& username,
                         const std::string& hostname) {
  if (is_unicode) {
    return authenticate_writer->WriteUtf16String(domain) &&
           authenticate_writer->WriteUtf16String(username) &&
           authenticate_writer->WriteUtf8AsUtf16String(hostname);
  } else {
    return authenticate_writer->WriteUtf16AsUtf8String(domain) &&
           authenticate_writer->WriteUtf16AsUtf8String(username) &&
           authenticate_writer->WriteUtf8String(hostname);
  }
}

// Returns the size in bytes of a string16 depending whether unicode
// was negotiated.
size_t GetStringPayloadLength(const std::u16string& str, bool is_unicode) {
  if (is_unicode)
    return str.length() * 2;

  // When |WriteUtf16AsUtf8String| is called with a |std::u16string|, the string
  // is converted to UTF8. Do the conversion to ensure that the character
  // count is correct.
  return base::UTF16ToUTF8(str).length();
}

// Returns the size in bytes of a std::string depending whether unicode
// was negotiated.
size_t GetStringPayloadLength(const std::string& str, bool is_unicode) {
  if (!is_unicode)
    return str.length();

  return base::UTF8ToUTF16(str).length() * 2;
}

// Sets |buffer| to point to |length| bytes from |offset| and updates |offset|
// past those bytes. In case of overflow, returns false.
bool ComputeSecurityBuffer(uint32_t* offset,
                           size_t length,
                           SecurityBuffer* buffer) {
  base::CheckedNumeric<uint16_t> length_checked = length;
  if (!length_checked.IsValid()) {
    return false;
  }
  base::CheckedNumeric<uint32_t> new_offset = *offset + length_checked;
  if (!new_offset.IsValid()) {
    return false;
  }
  buffer->offset = *offset;
  buffer->length = length_checked.ValueOrDie();
  *offset = new_offset.ValueOrDie();
  return true;
}

}  // namespace

NtlmClient::NtlmClient(NtlmFeatures features)
    : features_(features), negotiate_flags_(kNegotiateMessageFlags) {
  // Just generate the negotiate message once and hold on to it. It never
  // changes and in NTLMv2 it's used as an input to the Message Integrity
  // Check (MIC) in the Authenticate message.
  GenerateNegotiateMessage();
}

NtlmClient::~NtlmClient() = default;

std::vector<uint8_t> NtlmClient::GetNegotiateMessage() const {
  return negotiate_message_;
}

void NtlmClient::GenerateNegotiateMessage() {
  NtlmBufferWriter writer(kNegotiateMessageLen);
  bool result =
      writer.WriteMessageHeader(MessageType::kNegotiate) &&
      writer.WriteFlags(negotiate_flags_) &&
      writer.WriteSecurityBuffer(SecurityBuffer(kNegotiateMessageLen, 0)) &&
      writer.WriteSecurityBuffer(SecurityBuffer(kNegotiateMessageLen, 0)) &&
      writer.IsEndOfBuffer();

  DCHECK(result);

  negotiate_message_ = writer.Pass();
}

std::vector<uint8_t> NtlmClient::GenerateAuthenticateMessage(
    const std::u16string& domain,
    const std::u16string& username,
    const std::u16string& password,
    const std::string& hostname,
    const std::string& channel_bindings,
    const std::string& spn,
    uint64_t client_time,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<const uint8_t> server_challenge_message) const {
  // Limit the size of strings that are accepted. As an absolute limit any
  // field represented by a |SecurityBuffer| or |AvPair| must be less than
  // UINT16_MAX bytes long. The strings are restricted to the maximum sizes
  // without regard to encoding. As such this isn't intended to restrict all
  // invalid inputs, only to allow all possible valid inputs.
  //
  // |domain| and |hostname| can be no longer than 255 characters.
  // |username| can be no longer than 104 characters. See [1].
  // |password| can be no longer than 256 characters. See [2].
  //
  // [1] - https://technet.microsoft.com/en-us/library/bb726984.aspx
  // [2] - https://technet.microsoft.com/en-us/library/cc512606.aspx
  if (hostname.length() > kMaxFqdnLen || domain.length() > kMaxFqdnLen ||
      username.length() > kMaxUsernameLen ||
      password.length() > kMaxPasswordLen) {
    return {};
  }

  NegotiateFlags challenge_flags;
  uint8_t server_challenge[kChallengeLen];
  uint8_t lm_response[kResponseLenV1];
  uint8_t ntlm_response[kResponseLenV1];

  // Response fields only for NTLMv2
  std::vector<uint8_t> updated_target_info;
  std::vector<uint8_t> v2_proof_input;
  uint8_t v2_proof[kNtlmProofLenV2];
  uint8_t v2_session_key[kSessionKeyLenV2];

  if (IsNtlmV2()) {
    std::vector<AvPair> av_pairs;
    if (!ParseChallengeMessageV2(server_challenge_message, &challenge_flags,
                                 server_challenge, &av_pairs)) {
      return {};
    }

    uint64_t timestamp;
    updated_target_info =
        GenerateUpdatedTargetInfo(IsMicEnabled(), IsEpaEnabled(),
                                  channel_bindings, spn, av_pairs, &timestamp);

    memset(lm_response, 0, kResponseLenV1);
    if (timestamp == UINT64_MAX) {
      // If the server didn't send a time, then use the clients time.
      timestamp = client_time;
    }

    uint8_t v2_hash[kNtlmHashLen];
    GenerateNtlmHashV2(domain, username, password, v2_hash);
    v2_proof_input = GenerateProofInputV2(timestamp, client_challenge);
    GenerateNtlmProofV2(
        v2_hash, server_challenge,
        *base::span(v2_proof_input).to_fixed_extent<kProofInputLenV2>(),
        updated_target_info, v2_proof);
    GenerateSessionBaseKeyV2(v2_hash, v2_proof, v2_session_key);
  } else {
    if (!ParseChallengeMessage(server_challenge_message, &challenge_flags,
                               server_challenge)) {
      return {};
    }

    // Calculate the responses for the authenticate message.
    GenerateResponsesV1WithSessionSecurity(password, server_challenge,
                                           client_challenge, lm_response,
                                           ntlm_response);
  }

  // Always use extended session security even if the server tries to downgrade.
  NegotiateFlags authenticate_flags = (challenge_flags & negotiate_flags_) |
                                      NegotiateFlags::kExtendedSessionSecurity;

  // Calculate all the payload lengths and offsets.
  bool is_unicode = (authenticate_flags & NegotiateFlags::kUnicode) ==
                    NegotiateFlags::kUnicode;

  SecurityBuffer lm_info;
  SecurityBuffer ntlm_info;
  SecurityBuffer domain_info;
  SecurityBuffer username_info;
  SecurityBuffer hostname_info;
  SecurityBuffer session_key_info;
  size_t authenticate_message_len;

  if (!CalculatePayloadLayout(is_unicode, domain, username, hostname,
                              updated_target_info.size(), &lm_info, &ntlm_info,
                              &domain_info, &username_info, &hostname_info,
                              &session_key_info, &authenticate_message_len)) {
    return {};
  }

  NtlmBufferWriter authenticate_writer(authenticate_message_len);
  bool writer_result = WriteAuthenticateMessage(
      &authenticate_writer, lm_info, ntlm_info, domain_info, username_info,
      hostname_info, session_key_info, authenticate_flags);
  DCHECK(writer_result);

  if (IsNtlmV2()) {
    // Write the optional (for V1) Version and MIC fields. Note that they
    // could also safely be sent in V1. However, the server should never try to
    // read them, because neither the version negotiate flag nor the
    // |TargetInfoAvFlags::kMicPresent| in the target info are set.
    //
    // Version is never supported so it is filled with zeros. MIC is a hash
    // calculated over all 3 messages while the MIC is set to zeros then
    // backfilled at the end if the MIC feature is enabled.
    writer_result = authenticate_writer.WriteZeros(kVersionFieldLen) &&
                    authenticate_writer.WriteZeros(kMicLenV2);

    DCHECK(writer_result);
  }

  // Verify the location in the payload buffer.
  DCHECK(authenticate_writer.GetCursor() == GetAuthenticateHeaderLength());
  DCHECK(GetAuthenticateHeaderLength() == lm_info.offset);

  if (IsNtlmV2()) {
    // Write the response payloads for V2.
    writer_result =
        WriteResponsePayloadsV2(&authenticate_writer, lm_response, v2_proof,
                                v2_proof_input, updated_target_info);
  } else {
    // Write the response payloads.
    DCHECK_EQ(kResponseLenV1, lm_info.length);
    DCHECK_EQ(kResponseLenV1, ntlm_info.length);
    writer_result =
        WriteResponsePayloads(&authenticate_writer, lm_response, ntlm_response);
  }

  DCHECK(writer_result);
  DCHECK_EQ(authenticate_writer.GetCursor(), domain_info.offset);

  writer_result = WriteStringPayloads(&authenticate_writer, is_unicode, domain,
                                      username, hostname);
  DCHECK(writer_result);
  DCHECK(authenticate_writer.IsEndOfBuffer());
  DCHECK_EQ(authenticate_message_len, authenticate_writer.GetLength());

  std::vector<uint8_t> auth_msg = authenticate_writer.Pass();

  // Backfill the MIC if enabled.
  if (IsMicEnabled()) {
    // The MIC has to be generated over all 3 completed messages with the MIC
    // set to zeros.
    DCHECK_LT(kMicOffsetV2 + kMicLenV2, authenticate_message_len);

    base::span<uint8_t, kMicLenV2> mic(
        const_cast<uint8_t*>(auth_msg.data()) + kMicOffsetV2, kMicLenV2);
    GenerateMicV2(v2_session_key, negotiate_message_, server_challenge_message,
                  auth_msg, mic);
  }

  return auth_msg;
}

bool NtlmClient::CalculatePayloadLayout(
    bool is_unicode,
    const std::u16string& domain,
    const std::u16string& username,
    const std::string& hostname,
    size_t updated_target_info_len,
    SecurityBuffer* lm_info,
    SecurityBuffer* ntlm_info,
    SecurityBuffer* domain_info,
    SecurityBuffer* username_info,
    SecurityBuffer* hostname_info,
    SecurityBuffer* session_key_info,
    size_t* authenticate_message_len) const {
  uint32_t offset = GetAuthenticateHeaderLength();
  if (!ComputeSecurityBuffer(&offset, 0, session_key_info) ||
      !ComputeSecurityBuffer(&offset, kResponseLenV1, lm_info) ||
      !ComputeSecurityBuffer(
          &offset, GetNtlmResponseLength(updated_target_info_len), ntlm_info) ||
      !ComputeSecurityBuffer(
          &offset, GetStringPayloadLength(domain, is_unicode), domain_info) ||
      !ComputeSecurityBuffer(&offset,
                             GetStringPayloadLength(username, is_unicode),
                             username_info) ||
      !ComputeSecurityBuffer(&offset,
                             GetStringPayloadLength(hostname, is_unicode),
                             hostname_info)) {
    return false;
  }

  *authenticate_message_len = offset;
  return true;
}

size_t NtlmClient::GetAuthenticateHeaderLength() const {
  if (IsNtlmV2()) {
    return kAuthenticateHeaderLenV2;
  }

  return kAuthenticateHeaderLenV1;
}

size_t NtlmClient::GetNtlmResponseLength(size_t updated_target_info_len) const {
  if (IsNtlmV2()) {
    return kNtlmResponseHeaderLenV2 + updated_target_info_len + 4;
  }

  return kResponseLenV1;
}

}  // namespace net::ntlm

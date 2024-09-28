// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm.h"

#include <string.h>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/net_string_util.h"
#include "net/ntlm/ntlm_buffer_writer.h"
#include "net/ntlm/ntlm_constants.h"
#include "third_party/boringssl/src/include/openssl/des.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/md4.h"
#include "third_party/boringssl/src/include/openssl/md5.h"

namespace net::ntlm {

namespace {

// Takes the parsed target info in |av_pairs| and performs the following
// actions.
//
// 1) If a |TargetInfoAvId::kTimestamp| AvPair exists, |server_timestamp|
//    is set to the payload.
// 2) If |is_mic_enabled| is true, the existing |TargetInfoAvId::kFlags| AvPair
//    will have the |TargetInfoAvFlags::kMicPresent| bit set. If an existing
//    flags AvPair does not already exist, a new one is added with the value of
//    |TargetInfoAvFlags::kMicPresent|.
// 3) If |is_epa_enabled| is true, two new AvPair entries will be added to
//    |av_pairs|. The first will be of type |TargetInfoAvId::kChannelBindings|
//    and contains MD5(|channel_bindings|) as the payload. The second will be
//    of type |TargetInfoAvId::kTargetName| and contains |spn| as a little
//    endian UTF16 string.
// 4) Sets |target_info_len| to the size of |av_pairs| when serialized into
//    a payload.
void UpdateTargetInfoAvPairs(bool is_mic_enabled,
                             bool is_epa_enabled,
                             const std::string& channel_bindings,
                             const std::string& spn,
                             std::vector<AvPair>* av_pairs,
                             uint64_t* server_timestamp,
                             size_t* target_info_len) {
  // Do a pass to update flags and calculate current length and
  // pull out the server timestamp if it is there.
  *server_timestamp = UINT64_MAX;
  *target_info_len = 0;

  bool need_flags_added = is_mic_enabled;
  for (AvPair& pair : *av_pairs) {
    *target_info_len += pair.avlen + kAvPairHeaderLen;
    switch (pair.avid) {
      case TargetInfoAvId::kFlags:
        // The parsing phase already set the payload to the |flags| field.
        if (is_mic_enabled) {
          pair.flags = pair.flags | TargetInfoAvFlags::kMicPresent;
        }

        need_flags_added = false;
        break;
      case TargetInfoAvId::kTimestamp:
        // The parsing phase already set the payload to the |timestamp| field.
        *server_timestamp = pair.timestamp;
        break;
      case TargetInfoAvId::kEol:
      case TargetInfoAvId::kChannelBindings:
      case TargetInfoAvId::kTargetName:
        // The terminator, |kEol|, should already have been removed from the
        // end of the list and would have been rejected if it has been inside
        // the list. Additionally |kChannelBindings| and |kTargetName| pairs
        // would have been rejected during the initial parsing. See
        // |NtlmBufferReader::ReadTargetInfo|.
        NOTREACHED_IN_MIGRATION();
        break;
      default:
        // Ignore entries we don't care about.
        break;
    }
  }

  if (need_flags_added) {
    DCHECK(is_mic_enabled);
    AvPair flags_pair(TargetInfoAvId::kFlags, sizeof(uint32_t));
    flags_pair.flags = TargetInfoAvFlags::kMicPresent;

    av_pairs->push_back(flags_pair);
    *target_info_len += kAvPairHeaderLen + flags_pair.avlen;
  }

  if (is_epa_enabled) {
    std::vector<uint8_t> channel_bindings_hash(kChannelBindingsHashLen, 0);

    // Hash the channel bindings if they exist otherwise they remain zeros.
    if (!channel_bindings.empty()) {
      GenerateChannelBindingHashV2(
          channel_bindings, *base::span(channel_bindings_hash)
                                 .to_fixed_extent<kChannelBindingsHashLen>());
    }

    av_pairs->emplace_back(TargetInfoAvId::kChannelBindings,
                           std::move(channel_bindings_hash));

    // Convert the SPN to little endian unicode.
    std::u16string spn16 = base::UTF8ToUTF16(spn);
    NtlmBufferWriter spn_writer(spn16.length() * 2);
    bool spn_writer_result =
        spn_writer.WriteUtf16String(spn16) && spn_writer.IsEndOfBuffer();
    DCHECK(spn_writer_result);

    av_pairs->emplace_back(TargetInfoAvId::kTargetName, spn_writer.Pass());

    // Add the length of the two new AV Pairs to the total length.
    *target_info_len +=
        (2 * kAvPairHeaderLen) + kChannelBindingsHashLen + (spn16.length() * 2);
  }

  // Add extra space for the terminator at the end.
  *target_info_len += kAvPairHeaderLen;
}

std::vector<uint8_t> WriteUpdatedTargetInfo(const std::vector<AvPair>& av_pairs,
                                            size_t updated_target_info_len) {
  bool result = true;
  NtlmBufferWriter writer(updated_target_info_len);
  for (const AvPair& pair : av_pairs) {
    result = writer.WriteAvPair(pair);
    DCHECK(result);
  }

  result = writer.WriteAvPairTerminator() && writer.IsEndOfBuffer();
  DCHECK(result);
  return writer.Pass();
}

// Reads 7 bytes (56 bits) from |key_56| and writes them into 8 bytes of
// |key_64| with 7 bits in every byte. The least significant bits are
// undefined and a subsequent operation will set those bits with a parity bit.
// |key_56| must contain 7 bytes.
// |key_64| must contain 8 bytes.
void Splay56To64(base::span<const uint8_t, 7> key_56,
                 base::span<uint8_t, 8> key_64) {
  key_64[0] = key_56[0];
  key_64[1] = key_56[0] << 7 | key_56[1] >> 1;
  key_64[2] = key_56[1] << 6 | key_56[2] >> 2;
  key_64[3] = key_56[2] << 5 | key_56[3] >> 3;
  key_64[4] = key_56[3] << 4 | key_56[4] >> 4;
  key_64[5] = key_56[4] << 3 | key_56[5] >> 5;
  key_64[6] = key_56[5] << 2 | key_56[6] >> 6;
  key_64[7] = key_56[6] << 1;
}

}  // namespace

void Create3DesKeysFromNtlmHash(
    base::span<const uint8_t, kNtlmHashLen> ntlm_hash,
    base::span<uint8_t, 24> keys) {
  // Put the first 112 bits from |ntlm_hash| into the first 16 bytes of
  // |keys|.
  Splay56To64(ntlm_hash.first<7>(), keys.first<8>());
  Splay56To64(ntlm_hash.subspan<7, 7>(), keys.subspan<8, 8>());

  // Put the next 2x 7 bits in bytes 16 and 17 of |keys|, then
  // the last 2 bits in byte 18, then zero pad the rest of the final key.
  keys[16] = ntlm_hash[14];
  keys[17] = ntlm_hash[14] << 7 | ntlm_hash[15] >> 1;
  keys[18] = ntlm_hash[15] << 6;
  memset(keys.data() + 19, 0, 5);
}

void GenerateNtlmHashV1(const std::u16string& password,
                        base::span<uint8_t, kNtlmHashLen> hash) {
  size_t length = password.length() * 2;
  NtlmBufferWriter writer(length);

  // The writer will handle the big endian case if necessary.
  bool result = writer.WriteUtf16String(password) && writer.IsEndOfBuffer();
  DCHECK(result);

  MD4(writer.GetBuffer().data(), writer.GetLength(), hash.data());
}

void GenerateResponseDesl(base::span<const uint8_t, kNtlmHashLen> hash,
                          base::span<const uint8_t, kChallengeLen> challenge,
                          base::span<uint8_t, kResponseLenV1> response) {
  constexpr size_t block_count = 3;
  constexpr size_t block_size = sizeof(DES_cblock);
  static_assert(kChallengeLen == block_size,
                "kChallengeLen must equal block_size");
  static_assert(kResponseLenV1 == block_count * block_size,
                "kResponseLenV1 must equal block_count * block_size");

  const DES_cblock* challenge_block =
      reinterpret_cast<const DES_cblock*>(challenge.data());
  uint8_t keys[block_count * block_size];

  // Map the NTLM hash to three 8 byte DES keys, with 7 bits of the key in each
  // byte and the least significant bit set with odd parity. Then encrypt the
  // 8 byte challenge with each of the three keys. This produces three 8 byte
  // encrypted blocks into |response|.
  Create3DesKeysFromNtlmHash(hash, keys);
  for (size_t ix = 0; ix < block_count * block_size; ix += block_size) {
    DES_cblock* key_block = reinterpret_cast<DES_cblock*>(keys + ix);
    DES_cblock* response_block =
        reinterpret_cast<DES_cblock*>(response.data() + ix);

    DES_key_schedule key_schedule;
    DES_set_odd_parity(key_block);
    DES_set_key(key_block, &key_schedule);
    DES_ecb_encrypt(challenge_block, response_block, &key_schedule,
                    DES_ENCRYPT);
  }
}

void GenerateNtlmResponseV1(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<uint8_t, kResponseLenV1> ntlm_response) {
  uint8_t ntlm_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, ntlm_hash);
  GenerateResponseDesl(ntlm_hash, server_challenge, ntlm_response);
}

void GenerateResponsesV1(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response,
    base::span<uint8_t, kResponseLenV1> ntlm_response) {
  GenerateNtlmResponseV1(password, server_challenge, ntlm_response);

  // In NTLM v1 (with LMv1 disabled), the lm_response and ntlm_response are the
  // same. So just copy the ntlm_response into the lm_response.
  memcpy(lm_response.data(), ntlm_response.data(), kResponseLenV1);
}

void GenerateLMResponseV1WithSessionSecurity(
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response) {
  // In NTLM v1 with Session Security (aka NTLM2) the lm_response is 8 bytes of
  // client challenge and 16 bytes of zeros. (See 3.3.1)
  memcpy(lm_response.data(), client_challenge.data(), kChallengeLen);
  memset(lm_response.data() + kChallengeLen, 0, kResponseLenV1 - kChallengeLen);
}

void GenerateSessionHashV1WithSessionSecurity(
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kNtlmHashLen> session_hash) {
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, server_challenge.data(), kChallengeLen);
  MD5_Update(&ctx, client_challenge.data(), kChallengeLen);
  MD5_Final(session_hash.data(), &ctx);
}

void GenerateNtlmResponseV1WithSessionSecurity(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> ntlm_response) {
  // Generate the NTLMv1 Hash.
  uint8_t ntlm_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, ntlm_hash);

  // Generate the NTLMv1 Session Hash.
  uint8_t session_hash[kNtlmHashLen];
  GenerateSessionHashV1WithSessionSecurity(server_challenge, client_challenge,
                                           session_hash);

  GenerateResponseDesl(
      ntlm_hash, base::make_span(session_hash).subspan<0, kChallengeLen>(),
      ntlm_response);
}

void GenerateResponsesV1WithSessionSecurity(
    const std::u16string& password,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kChallengeLen> client_challenge,
    base::span<uint8_t, kResponseLenV1> lm_response,
    base::span<uint8_t, kResponseLenV1> ntlm_response) {
  GenerateLMResponseV1WithSessionSecurity(client_challenge, lm_response);
  GenerateNtlmResponseV1WithSessionSecurity(password, server_challenge,
                                            client_challenge, ntlm_response);
}

void GenerateNtlmHashV2(const std::u16string& domain,
                        const std::u16string& username,
                        const std::u16string& password,
                        base::span<uint8_t, kNtlmHashLen> v2_hash) {
  // NOTE: According to [MS-NLMP] Section 3.3.2 only the username and not the
  // domain is uppercased.

  // TODO(crbug.com/40674019): Using a locale-sensitive upper casing
  // algorithm is problematic. A more predictable approach would be to only
  // uppercase ASCII characters, so the hash does not change depending on the
  // user's locale.
  std::u16string upper_username;
  bool result = ToUpperUsingLocale(username, &upper_username);
  DCHECK(result);

  uint8_t v1_hash[kNtlmHashLen];
  GenerateNtlmHashV1(password, v1_hash);
  NtlmBufferWriter input_writer((upper_username.length() + domain.length()) *
                                2);
  bool writer_result = input_writer.WriteUtf16String(upper_username) &&
                       input_writer.WriteUtf16String(domain) &&
                       input_writer.IsEndOfBuffer();
  DCHECK(writer_result);

  unsigned int outlen = kNtlmHashLen;
  uint8_t* out_hash =
      HMAC(EVP_md5(), v1_hash, sizeof(v1_hash), input_writer.GetBuffer().data(),
           input_writer.GetLength(), v2_hash.data(), &outlen);
  DCHECK_EQ(v2_hash.data(), out_hash);
  DCHECK_EQ(sizeof(v1_hash), outlen);
}

std::vector<uint8_t> GenerateProofInputV2(
    uint64_t timestamp,
    base::span<const uint8_t, kChallengeLen> client_challenge) {
  NtlmBufferWriter writer(kProofInputLenV2);
  bool result = writer.WriteUInt16(kProofInputVersionV2) &&
                writer.WriteZeros(6) && writer.WriteUInt64(timestamp) &&
                writer.WriteBytes(client_challenge) && writer.WriteZeros(4) &&
                writer.IsEndOfBuffer();

  DCHECK(result);
  return writer.Pass();
}

void GenerateNtlmProofV2(
    base::span<const uint8_t, kNtlmHashLen> v2_hash,
    base::span<const uint8_t, kChallengeLen> server_challenge,
    base::span<const uint8_t, kProofInputLenV2> v2_input,
    base::span<const uint8_t> target_info,
    base::span<uint8_t, kNtlmProofLenV2> v2_proof) {
  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), v2_hash.data(), kNtlmHashLen, EVP_md5(), nullptr);
  DCHECK_EQ(kNtlmProofLenV2, HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), server_challenge.data(), kChallengeLen);
  HMAC_Update(ctx.get(), v2_input.data(), kProofInputLenV2);
  HMAC_Update(ctx.get(), target_info.data(), target_info.size());
  const uint32_t zero = 0;
  HMAC_Update(ctx.get(), reinterpret_cast<const uint8_t*>(&zero),
              sizeof(uint32_t));
  HMAC_Final(ctx.get(), v2_proof.data(), nullptr);
}

void GenerateSessionBaseKeyV2(
    base::span<const uint8_t, kNtlmHashLen> v2_hash,
    base::span<const uint8_t, kNtlmProofLenV2> v2_proof,
    base::span<uint8_t, kSessionKeyLenV2> session_key) {
  unsigned int outlen = kSessionKeyLenV2;
  uint8_t* result =
      HMAC(EVP_md5(), v2_hash.data(), kNtlmHashLen, v2_proof.data(),
           kNtlmProofLenV2, session_key.data(), &outlen);
  DCHECK_EQ(session_key.data(), result);
  DCHECK_EQ(kSessionKeyLenV2, outlen);
}

void GenerateChannelBindingHashV2(
    const std::string& channel_bindings,
    base::span<uint8_t, kNtlmHashLen> channel_bindings_hash) {
  NtlmBufferWriter writer(kEpaUnhashedStructHeaderLen);
  bool result = writer.WriteZeros(16) &&
                writer.WriteUInt32(channel_bindings.length()) &&
                writer.IsEndOfBuffer();
  DCHECK(result);

  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, writer.GetBuffer().data(), writer.GetBuffer().size());
  MD5_Update(&ctx, channel_bindings.data(), channel_bindings.size());
  MD5_Final(channel_bindings_hash.data(), &ctx);
}

void GenerateMicV2(base::span<const uint8_t, kSessionKeyLenV2> session_key,
                   base::span<const uint8_t> negotiate_msg,
                   base::span<const uint8_t> challenge_msg,
                   base::span<const uint8_t> authenticate_msg,
                   base::span<uint8_t, kMicLenV2> mic) {
  bssl::ScopedHMAC_CTX ctx;
  HMAC_Init_ex(ctx.get(), session_key.data(), kSessionKeyLenV2, EVP_md5(),
               nullptr);
  DCHECK_EQ(kMicLenV2, HMAC_size(ctx.get()));
  HMAC_Update(ctx.get(), negotiate_msg.data(), negotiate_msg.size());
  HMAC_Update(ctx.get(), challenge_msg.data(), challenge_msg.size());
  HMAC_Update(ctx.get(), authenticate_msg.data(), authenticate_msg.size());
  HMAC_Final(ctx.get(), mic.data(), nullptr);
}

NET_EXPORT_PRIVATE std::vector<uint8_t> GenerateUpdatedTargetInfo(
    bool is_mic_enabled,
    bool is_epa_enabled,
    const std::string& channel_bindings,
    const std::string& spn,
    const std::vector<AvPair>& av_pairs,
    uint64_t* server_timestamp) {
  size_t updated_target_info_len = 0;
  std::vector<AvPair> updated_av_pairs(av_pairs);
  UpdateTargetInfoAvPairs(is_mic_enabled, is_epa_enabled, channel_bindings, spn,
                          &updated_av_pairs, server_timestamp,
                          &updated_target_info_len);
  return WriteUpdatedTargetInfo(updated_av_pairs, updated_target_info_len);
}

}  // namespace net::ntlm

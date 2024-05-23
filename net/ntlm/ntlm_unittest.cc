// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

// Tests on exact results from cryptographic operations are based on test data
// provided in [MS-NLMP] Version 28.0 [1] Section 4.2.
//
// Additional sanity checks on the low level hashing operations test for
// properties of the outputs, such as whether the hashes change, whether they
// should be zeroed out, or whether they should be the same or different.
//
// [1] https://msdn.microsoft.com/en-us/library/cc236621.aspx

#include "net/ntlm/ntlm.h"

#include <iterator>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "net/ntlm/ntlm_test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::ntlm {

namespace {

AvPair MakeDomainAvPair() {
  return AvPair(TargetInfoAvId::kDomainName,
                std::vector<uint8_t>{std::begin(test::kNtlmDomainRaw),
                                     std::end(test::kNtlmDomainRaw)});
}

AvPair MakeServerAvPair() {
  return AvPair(TargetInfoAvId::kServerName,
                std::vector<uint8_t>{std::begin(test::kServerRaw),
                                     std::end(test::kServerRaw)});
}

// Clear the least significant bit in each byte.
void ClearLsb(base::span<uint8_t> data) {
  for (uint8_t& byte : data) {
    byte &= ~1;
  }
}

}  // namespace

TEST(NtlmTest, MapHashToDesKeysAllOnes) {
  // Test mapping an NTLM hash with all 1 bits.
  const uint8_t hash[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  const uint8_t expected[24] = {0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
                                0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
                                0xfe, 0xfe, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00};

  uint8_t result[24];
  Create3DesKeysFromNtlmHash(hash, result);
  // The least significant bit in result from |Create3DesKeysFromNtlmHash|
  // is undefined, so clear it to do memcmp.
  ClearLsb(result);

  EXPECT_TRUE(base::ranges::equal(expected, result));
}

TEST(NtlmTest, MapHashToDesKeysAllZeros) {
  // Test mapping an NTLM hash with all 0 bits.
  const uint8_t hash[16] = {0x00};
  const uint8_t expected[24] = {0x00};

  uint8_t result[24];
  Create3DesKeysFromNtlmHash(hash, result);
  // The least significant bit in result from |Create3DesKeysFromNtlmHash|
  // is undefined, so clear it to do memcmp.
  ClearLsb(result);

  EXPECT_TRUE(base::ranges::equal(expected, result));
}

TEST(NtlmTest, MapHashToDesKeysAlternatingBits) {
  // Test mapping an NTLM hash with alternating 0 and 1 bits.
  const uint8_t hash[16] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
                            0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
  const uint8_t expected[24] = {0xaa, 0x54, 0xaa, 0x54, 0xaa, 0x54, 0xaa, 0x54,
                                0xaa, 0x54, 0xaa, 0x54, 0xaa, 0x54, 0xaa, 0x54,
                                0xaa, 0x54, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};

  uint8_t result[24];
  Create3DesKeysFromNtlmHash(hash, result);
  // The least significant bit in result from |Create3DesKeysFromNtlmHash|
  // is undefined, so clear it to do memcmp.
  ClearLsb(result);

  EXPECT_TRUE(base::ranges::equal(expected, result));
}

TEST(NtlmTest, GenerateNtlmHashV1PasswordSpecTests) {
  uint8_t hash[kNtlmHashLen];
  GenerateNtlmHashV1(test::kPassword, hash);
  ASSERT_EQ(0, memcmp(hash, test::kExpectedNtlmHashV1, kNtlmHashLen));
}

TEST(NtlmTest, GenerateNtlmHashV1PasswordChangesHash) {
  std::u16string password1 = u"pwd01";
  std::u16string password2 = u"pwd02";
  uint8_t hash1[kNtlmHashLen];
  uint8_t hash2[kNtlmHashLen];

  GenerateNtlmHashV1(password1, hash1);
  GenerateNtlmHashV1(password2, hash2);

  // Verify that the hash is different with a different password.
  ASSERT_NE(0, memcmp(hash1, hash2, kNtlmHashLen));
}

TEST(NtlmTest, GenerateResponsesV1SpecTests) {
  uint8_t lm_response[kResponseLenV1];
  uint8_t ntlm_response[kResponseLenV1];
  GenerateResponsesV1(test::kPassword, test::kServerChallenge, lm_response,
                      ntlm_response);

  ASSERT_EQ(
      0, memcmp(test::kExpectedNtlmResponseV1, ntlm_response, kResponseLenV1));

  // This implementation never sends an LMv1 response (spec equivalent of the
  // client variable NoLMResponseNTLMv1 being false) so the LM response is
  // equal to the NTLM response when
  // NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY is not negotiated. See
  // [MS-NLMP] Section 3.3.1.
  ASSERT_EQ(0,
            memcmp(test::kExpectedNtlmResponseV1, lm_response, kResponseLenV1));
}

TEST(NtlmTest, GenerateResponsesV1WithSessionSecuritySpecTests) {
  uint8_t lm_response[kResponseLenV1];
  uint8_t ntlm_response[kResponseLenV1];
  GenerateResponsesV1WithSessionSecurity(
      test::kPassword, test::kServerChallenge, test::kClientChallenge,
      lm_response, ntlm_response);

  ASSERT_EQ(0, memcmp(test::kExpectedLmResponseWithV1SS, lm_response,
                      kResponseLenV1));
  ASSERT_EQ(0, memcmp(test::kExpectedNtlmResponseWithV1SS, ntlm_response,
                      kResponseLenV1));
}

TEST(NtlmTest, GenerateResponsesV1WithSessionSecurityClientChallengeUsed) {
  uint8_t lm_response1[kResponseLenV1];
  uint8_t lm_response2[kResponseLenV1];
  uint8_t ntlm_response1[kResponseLenV1];
  uint8_t ntlm_response2[kResponseLenV1];
  uint8_t client_challenge1[kChallengeLen];
  uint8_t client_challenge2[kChallengeLen];

  memset(client_challenge1, 0x01, kChallengeLen);
  memset(client_challenge2, 0x02, kChallengeLen);

  GenerateResponsesV1WithSessionSecurity(
      test::kPassword, test::kServerChallenge, client_challenge1, lm_response1,
      ntlm_response1);
  GenerateResponsesV1WithSessionSecurity(
      test::kPassword, test::kServerChallenge, client_challenge2, lm_response2,
      ntlm_response2);

  // The point of session security is that the client can introduce some
  // randomness, so verify different client_challenge gives a different result.
  ASSERT_NE(0, memcmp(lm_response1, lm_response2, kResponseLenV1));
  ASSERT_NE(0, memcmp(ntlm_response1, ntlm_response2, kResponseLenV1));

  // With session security the lm and ntlm hash should be different.
  ASSERT_NE(0, memcmp(lm_response1, ntlm_response1, kResponseLenV1));
  ASSERT_NE(0, memcmp(lm_response2, ntlm_response2, kResponseLenV1));
}

TEST(NtlmTest, GenerateResponsesV1WithSessionSecurityVerifySSUsed) {
  uint8_t lm_response1[kResponseLenV1];
  uint8_t lm_response2[kResponseLenV1];
  uint8_t ntlm_response1[kResponseLenV1];
  uint8_t ntlm_response2[kResponseLenV1];

  GenerateResponsesV1WithSessionSecurity(
      test::kPassword, test::kServerChallenge, test::kClientChallenge,
      lm_response1, ntlm_response1);
  GenerateResponsesV1(test::kPassword, test::kServerChallenge, lm_response2,
                      ntlm_response2);

  // Verify that the responses with session security are not the
  // same as without it.
  ASSERT_NE(0, memcmp(lm_response1, lm_response2, kResponseLenV1));
  ASSERT_NE(0, memcmp(ntlm_response1, ntlm_response2, kResponseLenV1));
}

// ------------------------------------------------
// NTLM V2 specific tests.
// ------------------------------------------------

TEST(NtlmTest, GenerateNtlmHashV2SpecTests) {
  uint8_t hash[kNtlmHashLen];
  GenerateNtlmHashV2(test::kNtlmDomain, test::kUser, test::kPassword, hash);
  ASSERT_EQ(0, memcmp(hash, test::kExpectedNtlmHashV2, kNtlmHashLen));
}

TEST(NtlmTest, GenerateProofInputV2SpecTests) {
  std::vector<uint8_t> proof_input;
  proof_input =
      GenerateProofInputV2(test::kServerTimestamp, test::kClientChallenge);
  ASSERT_EQ(kProofInputLenV2, proof_input.size());

  // |GenerateProofInputV2| generates the first |kProofInputLenV2| bytes of
  // what [MS-NLMP] calls "temp".
  ASSERT_EQ(0, memcmp(test::kExpectedTempFromSpecV2, proof_input.data(),
                      proof_input.size()));
}

TEST(NtlmTest, GenerateNtlmProofV2SpecTests) {
  // Only the first |kProofInputLenV2| bytes of |test::kExpectedTempFromSpecV2|
  // are read and this is equivalent to the output of |GenerateProofInputV2|.
  // See |GenerateProofInputV2SpecTests| for validation.
  uint8_t v2_proof[kNtlmProofLenV2];
  GenerateNtlmProofV2(test::kExpectedNtlmHashV2, test::kServerChallenge,
                      base::make_span(test::kExpectedTempFromSpecV2)
                          .subspan<0, kProofInputLenV2>(),
                      test::kExpectedTargetInfoFromSpecV2, v2_proof);

  ASSERT_EQ(0,
            memcmp(test::kExpectedProofFromSpecV2, v2_proof, kNtlmProofLenV2));
}

TEST(NtlmTest, GenerateSessionBaseKeyV2SpecTests) {
  // Generate the session base key.
  uint8_t session_base_key[kSessionKeyLenV2];
  GenerateSessionBaseKeyV2(test::kExpectedNtlmHashV2,
                           test::kExpectedProofFromSpecV2, session_base_key);

  // Verify the session base key.
  ASSERT_EQ(0, memcmp(test::kExpectedSessionBaseKeyFromSpecV2, session_base_key,
                      kSessionKeyLenV2));
}

TEST(NtlmTest, GenerateSessionBaseKeyWithClientTimestampV2SpecTests) {
  // Generate the session base key.
  uint8_t session_base_key[kSessionKeyLenV2];
  GenerateSessionBaseKeyV2(
      test::kExpectedNtlmHashV2,
      test::kExpectedProofSpecResponseWithClientTimestampV2, session_base_key);

  // Verify the session base key.
  ASSERT_EQ(0, memcmp(test::kExpectedSessionBaseKeyWithClientTimestampV2,
                      session_base_key, kSessionKeyLenV2));
}

TEST(NtlmTest, GenerateChannelBindingHashV2SpecTests) {
  uint8_t v2_channel_binding_hash[kChannelBindingsHashLen];
  GenerateChannelBindingHashV2(
      reinterpret_cast<const char*>(test::kChannelBindings),
      v2_channel_binding_hash);

  ASSERT_EQ(0, memcmp(test::kExpectedChannelBindingHashV2,
                      v2_channel_binding_hash, kChannelBindingsHashLen));
}

TEST(NtlmTest, GenerateMicV2Simple) {
  // The MIC is defined as HMAC_MD5(session_base_key, CONCAT(a, b, c)) where
  // a, b, c are the negotiate, challenge and authenticate messages
  // respectively.
  //
  // This compares a simple set of inputs to a precalculated result.
  const std::vector<uint8_t> a{0x44, 0x44, 0x44, 0x44};
  const std::vector<uint8_t> b{0x66, 0x66, 0x66, 0x66, 0x66, 0x66};
  const std::vector<uint8_t> c{0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};

  // expected_mic = HMAC_MD5(
  //          key=8de40ccadbc14a82f15cb0ad0de95ca3,
  //          input=444444446666666666668888888888888888)
  uint8_t expected_mic[kMicLenV2] = {0x71, 0xfe, 0xef, 0xd7, 0x76, 0xd4,
                                     0x42, 0xa8, 0x5f, 0x6e, 0x18, 0x0a,
                                     0x6b, 0x02, 0x47, 0x20};

  uint8_t mic[kMicLenV2];
  GenerateMicV2(test::kExpectedSessionBaseKeyFromSpecV2, a, b, c, mic);
  ASSERT_EQ(0, memcmp(expected_mic, mic, kMicLenV2));
}

TEST(NtlmTest, GenerateMicSpecResponseV2) {
  std::vector<uint8_t> authenticate_msg(
      std::begin(test::kExpectedAuthenticateMsgSpecResponseV2),
      std::end(test::kExpectedAuthenticateMsgSpecResponseV2));
  memset(&authenticate_msg[kMicOffsetV2], 0x00, kMicLenV2);

  uint8_t mic[kMicLenV2];
  GenerateMicV2(test::kExpectedSessionBaseKeyWithClientTimestampV2,
                test::kExpectedNegotiateMsg, test::kChallengeMsgFromSpecV2,
                authenticate_msg, mic);
  ASSERT_EQ(0, memcmp(test::kExpectedMicV2, mic, kMicLenV2));
}

TEST(NtlmTest, GenerateUpdatedTargetInfo) {
  // This constructs a std::vector<AvPair> that corresponds to the test input
  // values in [MS-NLMP] Section 4.2.4.
  std::vector<AvPair> server_av_pairs;
  server_av_pairs.push_back(MakeDomainAvPair());
  server_av_pairs.push_back(MakeServerAvPair());

  uint64_t server_timestamp = UINT64_MAX;
  std::vector<uint8_t> updated_target_info = GenerateUpdatedTargetInfo(
      true, true, reinterpret_cast<const char*>(test::kChannelBindings),
      test::kNtlmSpn, server_av_pairs, &server_timestamp);

  // With MIC and EPA enabled 3 additional AvPairs will be added.
  // 1) A flags AVPair with the MIC_PRESENT bit set.
  // 2) A channel bindings AVPair containing the channel bindings hash.
  // 3) A target name AVPair containing the SPN of the server.
  ASSERT_EQ(std::size(test::kExpectedTargetInfoSpecResponseV2),
            updated_target_info.size());
  ASSERT_EQ(0, memcmp(test::kExpectedTargetInfoSpecResponseV2,
                      updated_target_info.data(), updated_target_info.size()));
}

TEST(NtlmTest, GenerateUpdatedTargetInfoNoEpaOrMic) {
  // This constructs a std::vector<AvPair> that corresponds to the test input
  // values in [MS-NLMP] Section 4.2.4.
  std::vector<AvPair> server_av_pairs;
  server_av_pairs.push_back(MakeDomainAvPair());
  server_av_pairs.push_back(MakeServerAvPair());

  uint64_t server_timestamp = UINT64_MAX;

  // When both EPA and MIC are false the target info does not get modified by
  // the client.
  std::vector<uint8_t> updated_target_info = GenerateUpdatedTargetInfo(
      false, false, reinterpret_cast<const char*>(test::kChannelBindings),
      test::kNtlmSpn, server_av_pairs, &server_timestamp);
  ASSERT_EQ(std::size(test::kExpectedTargetInfoFromSpecV2),
            updated_target_info.size());
  ASSERT_EQ(0, memcmp(test::kExpectedTargetInfoFromSpecV2,
                      updated_target_info.data(), updated_target_info.size()));
}

TEST(NtlmTest, GenerateUpdatedTargetInfoWithServerTimestamp) {
  // This constructs a std::vector<AvPair> that corresponds to the test input
  // values in [MS-NLMP] Section 4.2.4 with an additional server timestamp.
  std::vector<AvPair> server_av_pairs;
  server_av_pairs.push_back(MakeDomainAvPair());
  server_av_pairs.push_back(MakeServerAvPair());

  // Set the timestamp to |test::kServerTimestamp| and the buffer to all zeros.
  AvPair pair(TargetInfoAvId::kTimestamp,
              std::vector<uint8_t>(sizeof(uint64_t), 0));
  pair.timestamp = test::kServerTimestamp;
  server_av_pairs.push_back(std::move(pair));

  uint64_t server_timestamp = UINT64_MAX;
  // When both EPA and MIC are false the target info does not get modified by
  // the client.
  std::vector<uint8_t> updated_target_info = GenerateUpdatedTargetInfo(
      false, false, reinterpret_cast<const char*>(test::kChannelBindings),
      test::kNtlmSpn, server_av_pairs, &server_timestamp);
  // Verify that the server timestamp was read from the target info.
  ASSERT_EQ(test::kServerTimestamp, server_timestamp);
  ASSERT_EQ(std::size(test::kExpectedTargetInfoFromSpecPlusServerTimestampV2),
            updated_target_info.size());
  ASSERT_EQ(0, memcmp(test::kExpectedTargetInfoFromSpecPlusServerTimestampV2,
                      updated_target_info.data(), updated_target_info.size()));
}

TEST(NtlmTest, GenerateUpdatedTargetInfoWhenServerSendsNoTargetInfo) {
  // In some older implementations the server supports NTLMv2 but does not
  // send target info. This manifests as an empty list of AvPairs.
  std::vector<AvPair> server_av_pairs;

  uint64_t server_timestamp = UINT64_MAX;
  std::vector<uint8_t> updated_target_info = GenerateUpdatedTargetInfo(
      true, true, reinterpret_cast<const char*>(test::kChannelBindings),
      test::kNtlmSpn, server_av_pairs, &server_timestamp);

  // With MIC and EPA enabled 3 additional AvPairs will be added.
  // 1) A flags AVPair with the MIC_PRESENT bit set.
  // 2) A channel bindings AVPair containing the channel bindings hash.
  // 3) A target name AVPair containing the SPN of the server.
  //
  // Compared to the spec example in |GenerateUpdatedTargetInfo| the result
  // is the same but with the first 32 bytes (which were the Domain and
  // Server pairs) not present.
  const size_t kMissingServerPairsLength = 32;

  ASSERT_EQ(std::size(test::kExpectedTargetInfoSpecResponseV2) -
                kMissingServerPairsLength,
            updated_target_info.size());
  ASSERT_EQ(0, memcmp(test::kExpectedTargetInfoSpecResponseV2 +
                          kMissingServerPairsLength,
                      updated_target_info.data(), updated_target_info.size()));
}

TEST(NtlmTest, GenerateNtlmProofV2) {
  uint8_t proof[kNtlmProofLenV2];

  GenerateNtlmProofV2(test::kExpectedNtlmHashV2, test::kServerChallenge,
                      base::make_span(test::kExpectedTempFromSpecV2)
                          .subspan<0, kProofInputLenV2>(),
                      test::kExpectedTargetInfoSpecResponseV2, proof);
  ASSERT_EQ(0,
            memcmp(test::kExpectedProofSpecResponseV2, proof, kNtlmProofLenV2));
}

TEST(NtlmTest, GenerateNtlmProofWithClientTimestampV2) {
  uint8_t proof[kNtlmProofLenV2];

  // Since the test data for "temp" in the spec does not include the client
  // timestamp, a separate proof test value must be validated for use in full
  // message validation.
  GenerateNtlmProofV2(test::kExpectedNtlmHashV2, test::kServerChallenge,
                      base::make_span(test::kExpectedTempWithClientTimestampV2)
                          .subspan<0, kProofInputLenV2>(),
                      test::kExpectedTargetInfoSpecResponseV2, proof);
  ASSERT_EQ(0, memcmp(test::kExpectedProofSpecResponseWithClientTimestampV2,
                      proof, kNtlmProofLenV2));
}

}  // namespace net::ntlm

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/ntlm/ntlm_client.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/ntlm/ntlm.h"
#include "net/ntlm/ntlm_buffer_reader.h"
#include "net/ntlm/ntlm_buffer_writer.h"
#include "net/ntlm/ntlm_test_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::ntlm {

namespace {

std::vector<uint8_t> GenerateAuthMsg(const NtlmClient& client,
                                     base::span<const uint8_t> challenge_msg) {
  return client.GenerateAuthenticateMessage(
      test::kNtlmDomain, test::kUser, test::kPassword, test::kHostnameAscii,
      reinterpret_cast<const char*>(test::kChannelBindings), test::kNtlmSpn,
      test::kClientTimestamp, test::kClientChallenge, challenge_msg);
}

std::vector<uint8_t> GenerateAuthMsg(const NtlmClient& client,
                                     const NtlmBufferWriter& challenge_writer) {
  return GenerateAuthMsg(client, challenge_writer.GetBuffer());
}

bool GetAuthMsgResult(const NtlmClient& client,
                      const NtlmBufferWriter& challenge_writer) {
  return !GenerateAuthMsg(client, challenge_writer).empty();
}

bool ReadBytesPayload(NtlmBufferReader* reader, base::span<uint8_t> buffer) {
  SecurityBuffer sec_buf;
  return reader->ReadSecurityBuffer(&sec_buf) &&
         (sec_buf.length == buffer.size()) &&
         reader->ReadBytesFrom(sec_buf, buffer);
}

// Reads bytes from a payload and assigns them to a string. This makes
// no assumptions about the underlying encoding.
bool ReadStringPayload(NtlmBufferReader* reader, std::string* str) {
  SecurityBuffer sec_buf;
  if (!reader->ReadSecurityBuffer(&sec_buf))
    return false;

  str->resize(sec_buf.length);
  if (!reader->ReadBytesFrom(sec_buf, base::as_writable_byte_span(*str))) {
    return false;
  }

  return true;
}

// Reads bytes from a payload and assigns them to a string16. This makes
// no assumptions about the underlying encoding. This will fail if there
// are an odd number of bytes in the payload.
bool ReadString16Payload(NtlmBufferReader* reader, std::u16string* str) {
  SecurityBuffer sec_buf;
  if (!reader->ReadSecurityBuffer(&sec_buf) || (sec_buf.length % 2 != 0))
    return false;

  std::vector<uint8_t> raw(sec_buf.length);
  if (!reader->ReadBytesFrom(sec_buf, raw))
    return false;

#if defined(ARCH_CPU_BIG_ENDIAN)
  for (size_t i = 0; i < raw.size(); i += 2) {
    std::swap(raw[i], raw[i + 1]);
  }
#endif

  str->assign(reinterpret_cast<const char16_t*>(raw.data()), raw.size() / 2);
  return true;
}

void MakeV2ChallengeMessage(size_t target_info_len, std::vector<uint8_t>* out) {
  static const size_t kChallengeV2HeaderLen = 56;

  // Leave room for the AV_PAIR header and the EOL pair.
  size_t server_name_len = target_info_len - kAvPairHeaderLen * 2;

  // See [MS-NLP] Section 2.2.1.2.
  NtlmBufferWriter challenge(kChallengeV2HeaderLen + target_info_len);
  ASSERT_TRUE(challenge.WriteMessageHeader(MessageType::kChallenge));
  ASSERT_TRUE(
      challenge.WriteSecurityBuffer(SecurityBuffer(0, 0)));  // target name
  ASSERT_TRUE(challenge.WriteFlags(NegotiateFlags::kTargetInfo));
  ASSERT_TRUE(challenge.WriteZeros(kChallengeLen));  // server challenge
  ASSERT_TRUE(challenge.WriteZeros(8));              // reserved
  ASSERT_TRUE(challenge.WriteSecurityBuffer(
      SecurityBuffer(kChallengeV2HeaderLen, target_info_len)));  // target info
  ASSERT_TRUE(challenge.WriteZeros(8));                          // version
  ASSERT_EQ(kChallengeV2HeaderLen, challenge.GetCursor());
  ASSERT_TRUE(challenge.WriteAvPair(
      AvPair(TargetInfoAvId::kServerName,
             std::vector<uint8_t>(server_name_len, 'a'))));
  ASSERT_TRUE(challenge.WriteAvPairTerminator());
  ASSERT_TRUE(challenge.IsEndOfBuffer());
  *out = challenge.Pass();
}

}  // namespace

TEST(NtlmClientTest, SimpleConstructionV1) {
  NtlmClient client(NtlmFeatures(false));

  ASSERT_FALSE(client.IsNtlmV2());
  ASSERT_FALSE(client.IsEpaEnabled());
  ASSERT_FALSE(client.IsMicEnabled());
}

TEST(NtlmClientTest, VerifyNegotiateMessageV1) {
  NtlmClient client(NtlmFeatures(false));

  std::vector<uint8_t> result = client.GetNegotiateMessage();

  ASSERT_EQ(kNegotiateMessageLen, result.size());
  ASSERT_EQ(0, memcmp(test::kExpectedNegotiateMsg, result.data(),
                      kNegotiateMessageLen));
}

TEST(NtlmClientTest, MinimalStructurallyValidChallenge) {
  NtlmClient client(NtlmFeatures(false));

  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(base::make_span(test::kMinChallengeMessage)
                                    .subspan<0, kMinChallengeHeaderLen>()));

  ASSERT_TRUE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, MinimalStructurallyValidChallengeZeroOffset) {
  NtlmClient client(NtlmFeatures(false));

  // The spec (2.2.1.2) states that the length SHOULD be 0 and the offset
  // SHOULD be where the payload would be if it was present. This is the
  // expected response from a compliant server when no target name is sent.
  // In reality the offset should always be ignored if the length is zero.
  // Also implementations often just write zeros.
  uint8_t raw[kMinChallengeHeaderLen];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Modify the default valid message to overwrite the offset to zero.
  ASSERT_NE(0x00, raw[16]);
  raw[16] = 0x00;

  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(raw));

  ASSERT_TRUE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, ChallengeMsgTooShort) {
  NtlmClient client(NtlmFeatures(false));

  // Fail because the minimum size valid message is 32 bytes.
  NtlmBufferWriter writer(kMinChallengeHeaderLen - 1);
  ASSERT_TRUE(writer.WriteBytes(base::make_span(test::kMinChallengeMessage)
                                    .subspan<0, kMinChallengeHeaderLen - 1>()));
  ASSERT_FALSE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, ChallengeMsgNoSig) {
  NtlmClient client(NtlmFeatures(false));

  // Fail because the first 8 bytes don't match "NTLMSSP\0"
  uint8_t raw[kMinChallengeHeaderLen];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Modify the default valid message to overwrite the last byte of the
  // signature.
  ASSERT_NE(0xff, raw[7]);
  raw[7] = 0xff;
  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(raw));
  ASSERT_FALSE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, ChallengeMsgWrongMessageType) {
  NtlmClient client(NtlmFeatures(false));

  // Fail because the message type should be MessageType::kChallenge
  // (0x00000002)
  uint8_t raw[kMinChallengeHeaderLen];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Modify the message type.
  ASSERT_NE(0x03, raw[8]);
  raw[8] = 0x03;

  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(raw));

  ASSERT_FALSE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, ChallengeWithNoTargetName) {
  NtlmClient client(NtlmFeatures(false));

  // The spec (2.2.1.2) states that the length SHOULD be 0 and the offset
  // SHOULD be where the payload would be if it was present. This is the
  // expected response from a compliant server when no target name is sent.
  // In reality the offset should always be ignored if the length is zero.
  // Also implementations often just write zeros.
  uint8_t raw[kMinChallengeHeaderLen];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Modify the default valid message to overwrite the offset to zero.
  ASSERT_NE(0x00, raw[16]);
  raw[16] = 0x00;

  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(raw));

  ASSERT_TRUE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, Type2MessageWithTargetName) {
  NtlmClient client(NtlmFeatures(false));

  // One extra byte is provided for target name.
  uint8_t raw[kMinChallengeHeaderLen + 1];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Put something in the target name.
  raw[kMinChallengeHeaderLen] = 'Z';

  // Modify the default valid message to indicate 1 byte is present in the
  // target name payload.
  ASSERT_NE(0x01, raw[12]);
  ASSERT_EQ(0x00, raw[13]);
  ASSERT_NE(0x01, raw[14]);
  ASSERT_EQ(0x00, raw[15]);
  raw[12] = 0x01;
  raw[14] = 0x01;

  NtlmBufferWriter writer(kChallengeHeaderLen + 1);
  ASSERT_TRUE(writer.WriteBytes(raw));
  ASSERT_TRUE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, NoTargetNameOverflowFromOffset) {
  NtlmClient client(NtlmFeatures(false));

  uint8_t raw[kMinChallengeHeaderLen];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Modify the default valid message to claim that the target name field is 1
  // byte long overrunning the end of the message message.
  ASSERT_NE(0x01, raw[12]);
  ASSERT_EQ(0x00, raw[13]);
  ASSERT_NE(0x01, raw[14]);
  ASSERT_EQ(0x00, raw[15]);
  raw[12] = 0x01;
  raw[14] = 0x01;

  NtlmBufferWriter writer(kMinChallengeHeaderLen);
  ASSERT_TRUE(writer.WriteBytes(raw));

  // The above malformed message could cause an implementation to read outside
  // the message buffer because the offset is past the end of the message.
  // Verify it gets rejected.
  ASSERT_FALSE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, NoTargetNameOverflowFromLength) {
  NtlmClient client(NtlmFeatures(false));

  // Message has 1 extra byte of space after the header for the target name.
  // One extra byte is provided for target name.
  uint8_t raw[kMinChallengeHeaderLen + 1];
  memcpy(raw, test::kMinChallengeMessage, kMinChallengeHeaderLen);
  // Put something in the target name.
  raw[kMinChallengeHeaderLen] = 'Z';

  // Modify the default valid message to indicate 2 bytes are present in the
  // target name payload (however there is only space for 1).
  ASSERT_NE(0x02, raw[12]);
  ASSERT_EQ(0x00, raw[13]);
  ASSERT_NE(0x02, raw[14]);
  ASSERT_EQ(0x00, raw[15]);
  raw[12] = 0x02;
  raw[14] = 0x02;

  NtlmBufferWriter writer(kMinChallengeHeaderLen + 1);
  ASSERT_TRUE(writer.WriteBytes(raw));

  // The above malformed message could cause an implementation
  // to read outside the message buffer because the length is
  // longer than available space. Verify it gets rejected.
  ASSERT_FALSE(GetAuthMsgResult(client, writer));
}

TEST(NtlmClientTest, Type3UnicodeWithSessionSecuritySpecTest) {
  NtlmClient client(NtlmFeatures(false));

  std::vector<uint8_t> result = GenerateAuthMsg(client, test::kChallengeMsgV1);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(std::size(test::kExpectedAuthenticateMsgSpecResponseV1),
            result.size());
  ASSERT_EQ(0, memcmp(test::kExpectedAuthenticateMsgSpecResponseV1,
                      result.data(), result.size()));
}

TEST(NtlmClientTest, Type3WithoutUnicode) {
  NtlmClient client(NtlmFeatures(false));

  std::vector<uint8_t> result = GenerateAuthMsg(
      client, base::make_span(test::kMinChallengeMessageNoUnicode)
                  .subspan<0, kMinChallengeHeaderLen>());
  ASSERT_FALSE(result.empty());

  NtlmBufferReader reader(result);
  ASSERT_TRUE(reader.MatchMessageHeader(MessageType::kAuthenticate));

  // Read the LM and NTLM Response Payloads.
  uint8_t actual_lm_response[kResponseLenV1];
  uint8_t actual_ntlm_response[kResponseLenV1];

  ASSERT_TRUE(ReadBytesPayload(&reader, actual_lm_response));
  ASSERT_TRUE(ReadBytesPayload(&reader, actual_ntlm_response));

  ASSERT_EQ(0, memcmp(test::kExpectedLmResponseWithV1SS, actual_lm_response,
                      kResponseLenV1));
  ASSERT_EQ(0, memcmp(test::kExpectedNtlmResponseWithV1SS, actual_ntlm_response,
                      kResponseLenV1));

  std::string domain;
  std::string username;
  std::string hostname;
  ASSERT_TRUE(ReadStringPayload(&reader, &domain));
  ASSERT_EQ(test::kNtlmDomainAscii, domain);
  ASSERT_TRUE(ReadStringPayload(&reader, &username));
  ASSERT_EQ(test::kUserAscii, username);
  ASSERT_TRUE(ReadStringPayload(&reader, &hostname));
  ASSERT_EQ(test::kHostnameAscii, hostname);

  // The session key is not used in HTTP. Since NTLMSSP_NEGOTIATE_KEY_EXCH
  // was not sent this is empty.
  ASSERT_TRUE(reader.MatchEmptySecurityBuffer());

  // Verify the unicode flag is not set and OEM flag is.
  NegotiateFlags flags;
  ASSERT_TRUE(reader.ReadFlags(&flags));
  ASSERT_EQ(NegotiateFlags::kNone, flags & NegotiateFlags::kUnicode);
  ASSERT_EQ(NegotiateFlags::kOem, flags & NegotiateFlags::kOem);
}

TEST(NtlmClientTest, ClientDoesNotDowngradeSessionSecurity) {
  NtlmClient client(NtlmFeatures(false));

  std::vector<uint8_t> result =
      GenerateAuthMsg(client, base::make_span(test::kMinChallengeMessageNoSS)
                                  .subspan<0, kMinChallengeHeaderLen>());
  ASSERT_FALSE(result.empty());

  NtlmBufferReader reader(result);
  ASSERT_TRUE(reader.MatchMessageHeader(MessageType::kAuthenticate));

  // Read the LM and NTLM Response Payloads.
  uint8_t actual_lm_response[kResponseLenV1];
  uint8_t actual_ntlm_response[kResponseLenV1];

  ASSERT_TRUE(ReadBytesPayload(&reader, actual_lm_response));
  ASSERT_TRUE(ReadBytesPayload(&reader, actual_ntlm_response));

  // The important part of this test is that even though the
  // server told the client to drop session security. The client
  // DID NOT drop it.
  ASSERT_EQ(0, memcmp(test::kExpectedLmResponseWithV1SS, actual_lm_response,
                      kResponseLenV1));
  ASSERT_EQ(0, memcmp(test::kExpectedNtlmResponseWithV1SS, actual_ntlm_response,
                      kResponseLenV1));

  std::u16string domain;
  std::u16string username;
  std::u16string hostname;
  ASSERT_TRUE(ReadString16Payload(&reader, &domain));
  ASSERT_EQ(test::kNtlmDomain, domain);
  ASSERT_TRUE(ReadString16Payload(&reader, &username));
  ASSERT_EQ(test::kUser, username);
  ASSERT_TRUE(ReadString16Payload(&reader, &hostname));
  ASSERT_EQ(test::kHostname, hostname);

  // The session key is not used in HTTP. Since NTLMSSP_NEGOTIATE_KEY_EXCH
  // was not sent this is empty.
  ASSERT_TRUE(reader.MatchEmptySecurityBuffer());

  // Verify the unicode and session security flag is set.
  NegotiateFlags flags;
  ASSERT_TRUE(reader.ReadFlags(&flags));
  ASSERT_EQ(NegotiateFlags::kUnicode, flags & NegotiateFlags::kUnicode);
  ASSERT_EQ(NegotiateFlags::kExtendedSessionSecurity,
            flags & NegotiateFlags::kExtendedSessionSecurity);
}

// ------------------------------------------------
// NTLM V2 specific tests.
// ------------------------------------------------

TEST(NtlmClientTest, SimpleConstructionV2) {
  NtlmClient client(NtlmFeatures(true));

  ASSERT_TRUE(client.IsNtlmV2());
  ASSERT_TRUE(client.IsEpaEnabled());
  ASSERT_TRUE(client.IsMicEnabled());
}

TEST(NtlmClientTest, VerifyNegotiateMessageV2) {
  NtlmClient client(NtlmFeatures(true));

  std::vector<uint8_t> result = client.GetNegotiateMessage();
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(std::size(test::kExpectedNegotiateMsg), result.size());
  ASSERT_EQ(0,
            memcmp(test::kExpectedNegotiateMsg, result.data(), result.size()));
}

TEST(NtlmClientTest, VerifyAuthenticateMessageV2) {
  // Generate the auth message from the client based on the test challenge
  // message.
  NtlmClient client(NtlmFeatures(true));
  std::vector<uint8_t> result =
      GenerateAuthMsg(client, test::kChallengeMsgFromSpecV2);
  ASSERT_FALSE(result.empty());
  ASSERT_EQ(std::size(test::kExpectedAuthenticateMsgSpecResponseV2),
            result.size());
  ASSERT_EQ(0, memcmp(test::kExpectedAuthenticateMsgSpecResponseV2,
                      result.data(), result.size()));
}

TEST(NtlmClientTest,
     VerifyAuthenticateMessageInResponseToChallengeWithoutTargetInfoV2) {
  // Test how the V2 client responds when the server sends a challenge that
  // does not contain target info. eg. Windows 2003 and earlier do not send
  // this. See [MS-NLMP] Appendix B Item 8. These older Windows servers
  // support NTLMv2 but don't send target info. Other implementations may
  // also be affected.
  NtlmClient client(NtlmFeatures(true));
  std::vector<uint8_t> result = GenerateAuthMsg(client, test::kChallengeMsgV1);
  ASSERT_FALSE(result.empty());

  ASSERT_EQ(std::size(test::kExpectedAuthenticateMsgToOldV1ChallegeV2),
            result.size());
  ASSERT_EQ(0, memcmp(test::kExpectedAuthenticateMsgToOldV1ChallegeV2,
                      result.data(), result.size()));
}

// When the challenge message's target info is maximum size, adding new AV_PAIRs
// to the response will overflow SecurityBuffer. Test that we handle this.
TEST(NtlmClientTest, AvPairsOverflow) {
  {
    NtlmClient client(NtlmFeatures(/*enable_NTLMv2=*/true));
    std::vector<uint8_t> short_challenge;
    ASSERT_NO_FATAL_FAILURE(MakeV2ChallengeMessage(0xfff, &short_challenge));
    EXPECT_FALSE(GenerateAuthMsg(client, short_challenge).empty());
  }
  {
    NtlmClient client(NtlmFeatures(/*enable_NTLMv2=*/true));
    std::vector<uint8_t> long_challenge;
    ASSERT_NO_FATAL_FAILURE(MakeV2ChallengeMessage(0xffff, &long_challenge));
    EXPECT_TRUE(GenerateAuthMsg(client, long_challenge).empty());
  }
}

}  // namespace net::ntlm

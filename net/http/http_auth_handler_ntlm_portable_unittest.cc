// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/test_completion_callback.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_request_info.h"
#include "net/http/mock_allow_http_auth_preferences.h"
#include "net/log/net_log_with_source.h"
#include "net/ntlm/ntlm.h"
#include "net/ntlm/ntlm_buffer_reader.h"
#include "net/ntlm/ntlm_buffer_writer.h"
#include "net/ntlm/ntlm_test_data.h"
#include "net/ssl/ssl_info.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class HttpAuthHandlerNtlmPortableTest : public PlatformTest {
 public:
  // Test input value defined in [MS-NLMP] Section 4.2.1.
  HttpAuthHandlerNtlmPortableTest() {
    http_auth_preferences_.reset(new MockAllowHttpAuthPreferences());
    // Disable NTLMv2 for this end to end test because it's not possible
    // to mock all the required dependencies for NTLMv2 from here. These
    // tests are only of the overall flow, and the detailed tests of the
    // contents of the protocol messages are in ntlm_client_unittest.cc
    http_auth_preferences_->set_ntlm_v2_enabled(false);
    factory_.reset(new HttpAuthHandlerNTLM::Factory());
    factory_->set_http_auth_preferences(http_auth_preferences_.get());
    creds_ = AuthCredentials(
        ntlm::test::kNtlmDomain + base::ASCIIToUTF16("\\") + ntlm::test::kUser,
        ntlm::test::kPassword);
  }

  int CreateHandler() {
    GURL gurl("https://foo.com");
    SSLInfo null_ssl_info;

    return factory_->CreateAuthHandlerFromString(
        "NTLM", HttpAuth::AUTH_SERVER, null_ssl_info, gurl, NetLogWithSource(),
        nullptr, &auth_handler_);
  }

  std::string CreateNtlmAuthHeader(base::span<const uint8_t> buffer) {
    std::string output;
    base::Base64Encode(
        base::StringPiece(reinterpret_cast<const char*>(buffer.data()),
                          buffer.size()),
        &output);

    return "NTLM " + output;
  }


  HttpAuth::AuthorizationResult HandleAnotherChallenge(
      const std::string& challenge) {
    HttpAuthChallengeTokenizer tokenizer(challenge.begin(), challenge.end());
    return GetAuthHandler()->HandleAnotherChallenge(&tokenizer);
  }

  bool DecodeChallenge(const std::string& challenge, std::string* decoded) {
    HttpAuthChallengeTokenizer tokenizer(challenge.begin(), challenge.end());
    return base::Base64Decode(tokenizer.base64_param(), decoded);
  }

  int GenerateAuthToken(std::string* token) {
    TestCompletionCallback callback;
    HttpRequestInfo request_info;
    return callback.GetResult(GetAuthHandler()->GenerateAuthToken(
        GetCreds(), &request_info, callback.callback(), token));
  }

  bool ReadBytesPayload(ntlm::NtlmBufferReader* reader,
                        base::span<uint8_t> buffer) {
    ntlm::SecurityBuffer sec_buf;
    return reader->ReadSecurityBuffer(&sec_buf) &&
           (sec_buf.length == buffer.size()) &&
           reader->ReadBytesFrom(sec_buf, buffer);
  }

  // Reads bytes from a payload and assigns them to a string. This makes
  // no assumptions about the underlying encoding.
  bool ReadStringPayload(ntlm::NtlmBufferReader* reader, std::string* str) {
    ntlm::SecurityBuffer sec_buf;
    if (!reader->ReadSecurityBuffer(&sec_buf))
      return false;

    if (!reader->ReadBytesFrom(
            sec_buf,
            base::as_writable_bytes(base::make_span(
                base::WriteInto(str, sec_buf.length + 1), sec_buf.length)))) {
      return false;
    }

    return true;
  }

  // Reads bytes from a payload and assigns them to a string16. This makes
  // no assumptions about the underlying encoding. This will fail if there
  // are an odd number of bytes in the payload.
  void ReadString16Payload(ntlm::NtlmBufferReader* reader,
                           base::string16* str) {
    ntlm::SecurityBuffer sec_buf;
    EXPECT_TRUE(reader->ReadSecurityBuffer(&sec_buf));
    EXPECT_EQ(0, sec_buf.length % 2);

    std::vector<uint8_t> raw(sec_buf.length);
    EXPECT_TRUE(reader->ReadBytesFrom(sec_buf, raw));

#if defined(ARCH_CPU_BIG_ENDIAN)
    for (size_t i = 0; i < raw.size(); i += 2) {
      std::swap(raw[i], raw[i + 1]);
    }
#endif

    str->assign(reinterpret_cast<const base::char16*>(raw.data()),
                raw.size() / 2);
  }

  int GetGenerateAuthTokenResult() {
    std::string token;
    return GenerateAuthToken(&token);
  }

  AuthCredentials* GetCreds() { return &creds_; }

  HttpAuthHandlerNTLM* GetAuthHandler() {
    return static_cast<HttpAuthHandlerNTLM*>(auth_handler_.get());
  }

  static void MockRandom(uint8_t* output, size_t n) {
    // This is set to 0xaa because the client challenge for testing in
    // [MS-NLMP] Section 4.2.1 is 8 bytes of 0xaa.
    memset(output, 0xaa, n);
  }

  static uint64_t MockGetMSTime() {
    // Tue, 23 May 2017 20:13:07 +0000
    return 131400439870000000;
  }

  static std::string MockGetHostName() { return ntlm::test::kHostnameAscii; }

 private:
  AuthCredentials creds_;
  std::unique_ptr<HttpAuthHandler> auth_handler_;
  std::unique_ptr<MockAllowHttpAuthPreferences> http_auth_preferences_;
  std::unique_ptr<HttpAuthHandlerNTLM::Factory> factory_;
};

TEST_F(HttpAuthHandlerNtlmPortableTest, SimpleConstruction) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_TRUE(GetAuthHandler() != nullptr);
}

TEST_F(HttpAuthHandlerNtlmPortableTest, DoNotAllowDefaultCreds) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_FALSE(GetAuthHandler()->AllowsDefaultCredentials());
}

TEST_F(HttpAuthHandlerNtlmPortableTest, AllowsExplicitCredentials) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_TRUE(GetAuthHandler()->AllowsExplicitCredentials());
}

TEST_F(HttpAuthHandlerNtlmPortableTest, VerifyType1Message) {
  ASSERT_EQ(OK, CreateHandler());

  std::string token;
  ASSERT_EQ(OK, GenerateAuthToken(&token));
  // The type 1 message generated is always the same. The only variable
  // part of the message is the flags and this implementation always offers
  // the same set of flags.
  ASSERT_EQ("NTLM TlRMTVNTUAABAAAAB4IIAAAAAAAgAAAAAAAAACAAAAA=", token);
}

TEST_F(HttpAuthHandlerNtlmPortableTest, EmptyTokenFails) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_EQ(OK, GetGenerateAuthTokenResult());

  // The encoded token for a type 2 message can't be empty.
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            HandleAnotherChallenge("NTLM"));
}

TEST_F(HttpAuthHandlerNtlmPortableTest, InvalidBase64Encoding) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_EQ(OK, GetGenerateAuthTokenResult());

  // Token isn't valid base64.
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            HandleAnotherChallenge("NTLM !!!!!!!!!!!!!"));
}

TEST_F(HttpAuthHandlerNtlmPortableTest, CantChangeSchemeMidway) {
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_EQ(OK, GetGenerateAuthTokenResult());

  // Can't switch to a different auth scheme in the middle of the process.
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            HandleAnotherChallenge("Negotiate SSdtIG5vdCBhIHJlYWwgdG9rZW4h"));
}

TEST_F(HttpAuthHandlerNtlmPortableTest, NtlmV1AuthenticationSuccess) {
  HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGetMSTime, MockRandom,
                                                    MockGetHostName);
  ASSERT_EQ(OK, CreateHandler());
  ASSERT_EQ(OK, GetGenerateAuthTokenResult());

  std::string token;
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            HandleAnotherChallenge(
                CreateNtlmAuthHeader(ntlm::test::kChallengeMsgV1)));
  ASSERT_EQ(OK, GenerateAuthToken(&token));

  // Validate the authenticate message
  std::string decoded;
  ASSERT_TRUE(DecodeChallenge(token, &decoded));
  ASSERT_EQ(base::size(ntlm::test::kExpectedAuthenticateMsgSpecResponseV1),
            decoded.size());
  ASSERT_EQ(0, memcmp(decoded.data(),
                      ntlm::test::kExpectedAuthenticateMsgSpecResponseV1,
                      decoded.size()));
}

}  // namespace net

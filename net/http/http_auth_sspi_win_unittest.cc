// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_sspi_win.h"

#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_challenge_tokenizer.h"
#include "net/http/mock_sspi_library_win.h"
#include "net/log/net_log_entry.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/log/test_net_log.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::IsError;
using net::test::IsOk;

namespace net {

namespace {

void MatchDomainUserAfterSplit(const std::u16string& combined,
                               const std::u16string& expected_domain,
                               const std::u16string& expected_user) {
  std::u16string actual_domain;
  std::u16string actual_user;
  SplitDomainAndUser(combined, &actual_domain, &actual_user);
  EXPECT_EQ(expected_domain, actual_domain);
  EXPECT_EQ(expected_user, actual_user);
}

const ULONG kMaxTokenLength = 100;

void UnexpectedCallback(int result) {
  // At present getting tokens from gssapi is fully synchronous, so the callback
  // should never be called.
  ADD_FAILURE();
}

}  // namespace

TEST(HttpAuthSSPITest, SplitUserAndDomain) {
  MatchDomainUserAfterSplit(u"foobar", u"", u"foobar");
  MatchDomainUserAfterSplit(u"FOO\\bar", u"FOO", u"bar");
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_Normal) {
  SecPkgInfoW package_info;
  memset(&package_info, 0x0, sizeof(package_info));
  package_info.cbMaxToken = 1337;

  MockSSPILibrary mock_library{L"NTLM"};
  mock_library.ExpectQuerySecurityPackageInfo(SEC_E_OK, &package_info);
  ULONG max_token_length = kMaxTokenLength;
  int rv = mock_library.DetermineMaxTokenLength(&max_token_length);
  EXPECT_THAT(rv, IsOk());
  EXPECT_EQ(1337u, max_token_length);
}

TEST(HttpAuthSSPITest, DetermineMaxTokenLength_InvalidPackage) {
  MockSSPILibrary mock_library{L"Foo"};
  mock_library.ExpectQuerySecurityPackageInfo(SEC_E_SECPKG_NOT_FOUND, nullptr);
  ULONG max_token_length = kMaxTokenLength;
  int rv = mock_library.DetermineMaxTokenLength(&max_token_length);
  EXPECT_THAT(rv, IsError(ERR_UNSUPPORTED_AUTH_SCHEME));
  // |DetermineMaxTokenLength()| interface states that |max_token_length| should
  // not change on failure.
  EXPECT_EQ(100u, max_token_length);
}

TEST(HttpAuthSSPITest, ParseChallenge_FirstRound) {
  // The first round should just consist of an unadorned "Negotiate" header.
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_TwoRounds) {
  // The first round should just have "Negotiate", and the second round should
  // have a valid base64 token associated with it.
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  // Generate an auth token and create another thing.
  std::string auth_token;
  EXPECT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));

  HttpAuthChallengeTokenizer second_challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_UnexpectedTokenFirstRound) {
  // If the first round challenge has an additional authentication token, it
  // should be treated as an invalid challenge from the server.
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer challenge("Negotiate Zm9vYmFy");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_MissingTokenSecondRound) {
  // If a later-round challenge is simply "Negotiate", it should be treated as
  // an authentication challenge rejection from the server or proxy.
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  HttpAuthChallengeTokenizer second_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_REJECT,
            auth_sspi.ParseChallenge(&second_challenge));
}

TEST(HttpAuthSSPITest, ParseChallenge_NonBase64EncodedToken) {
  // If a later-round challenge has an invalid base64 encoded token, it should
  // be treated as an invalid challenge.
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  EXPECT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  HttpAuthChallengeTokenizer second_challenge("Negotiate =happyjoy=");
  EXPECT_EQ(HttpAuth::AUTHORIZATION_RESULT_INVALID,
            auth_sspi.ParseChallenge(&second_challenge));
}

// Runs through a full handshake against the MockSSPILibrary.
TEST(HttpAuthSSPITest, GenerateAuthToken_FullHandshake_AmbientCreds) {
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  std::string first_challenge_text = "Negotiate";
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  EXPECT_EQ("Negotiate ", auth_token.substr(0, 10));

  std::string decoded_token;
  ASSERT_TRUE(base::Base64Decode(auth_token.substr(10), &decoded_token));

  // This token string indicates that HttpAuthSSPI correctly established the
  // security context using the default credentials.
  EXPECT_EQ("<Default>'s token #1 for HTTP/intranet.google.com", decoded_token);

  // The server token is arbitrary.
  HttpAuthChallengeTokenizer second_challenge("Negotiate UmVzcG9uc2U=");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));

  ASSERT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                NetLogWithSource(), base::BindOnce(&UnexpectedCallback)));
  ASSERT_EQ("Negotiate ", auth_token.substr(0, 10));
  ASSERT_TRUE(base::Base64Decode(auth_token.substr(10), &decoded_token));
  EXPECT_EQ("<Default>'s token #2 for HTTP/intranet.google.com", decoded_token);
}

// Test NetLogs produced while going through a full Negotiate handshake.
TEST(HttpAuthSSPITest, GenerateAuthToken_FullHandshake_AmbientCreds_Logging) {
  RecordingNetLogObserver net_log_observer;
  NetLogWithSource net_log_with_source =
      NetLogWithSource::Make(NetLogSourceType::NONE);
  MockSSPILibrary mock_library{NEGOSSP_NAME};
  HttpAuthSSPI auth_sspi(&mock_library, HttpAuth::AUTH_SCHEME_NEGOTIATE);
  HttpAuthChallengeTokenizer first_challenge("Negotiate");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&first_challenge));

  std::string auth_token;
  ASSERT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                net_log_with_source, base::BindOnce(&UnexpectedCallback)));

  // The token is the ASCII string "Response" in base64.
  HttpAuthChallengeTokenizer second_challenge("Negotiate UmVzcG9uc2U=");
  ASSERT_EQ(HttpAuth::AUTHORIZATION_RESULT_ACCEPT,
            auth_sspi.ParseChallenge(&second_challenge));
  ASSERT_EQ(OK,
            auth_sspi.GenerateAuthToken(
                nullptr, "HTTP/intranet.google.com", std::string(), &auth_token,
                net_log_with_source, base::BindOnce(&UnexpectedCallback)));

  auto entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_ACQUIRE_CREDS);
  ASSERT_EQ(2u, entries.size());  // BEGIN and END.
  auto expected = base::JSONReader::Read(R"(
    {
      "status": {
        "net_error": 0,
        "security_status": 0
       }
    }
  )");
  EXPECT_EQ(expected, entries[1].params);

  entries = net_log_observer.GetEntriesWithType(
      NetLogEventType::AUTH_LIBRARY_INIT_SEC_CTX);
  ASSERT_EQ(4u, entries.size());

  expected = base::JSONReader::Read(R"(
    {
       "flags": {
          "delegated": false,
          "mutual": false,
          "value": "0x00000000"
       },
       "spn": "HTTP/intranet.google.com"
    }
  )");
  EXPECT_EQ(expected, entries[0].params);

  expected = base::JSONReader::Read(R"(
    {
      "context": {
         "authority": "Dodgy Server",
         "flags": {
            "delegated": false,
            "mutual": false,
            "value": "0x00000000"
         },
         "mechanism": "Itsa me Kerberos!!",
         "open": true,
         "source": "\u003CDefault>",
         "target": "HTTP/intranet.google.com"
      },
      "status": {
         "net_error": 0,
         "security_status": 0
      }
    }
  )");
  EXPECT_EQ(expected, entries[1].params);

  expected = base::JSONReader::Read(R"(
    {
      "context": {
        "authority": "Dodgy Server",
        "flags": {
           "delegated": false,
           "mutual": false,
           "value": "0x00000000"
        },
        "mechanism": "Itsa me Kerberos!!",
        "open": false,
        "source": "\u003CDefault>",
        "target": "HTTP/intranet.google.com"
      },
      "status": {
         "net_error": 0,
         "security_status": 0
      }
    }
  )");
  EXPECT_EQ(expected, entries[3].params);
}
}  // namespace net

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"

#include <optional>

#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

constexpr char kRegistrationHeader[] = "Sec-Session-Registration";
using crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
using crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
using ::testing::UnorderedElementsAre;

scoped_refptr<net::HttpResponseHeaders> CreateHeaders(
    std::optional<std::string> path,
    std::optional<std::string> algs,
    std::optional<std::string> challenge,
    scoped_refptr<net::HttpResponseHeaders> headers = nullptr) {
  const std::string algs_string = (algs && !algs->empty()) ? *algs : "()";
  const std::string path_string =
      path ? base::StrCat({";path=\"", *path, "\""}) : "";
  const std::string challenge_string =
      challenge ? base::StrCat({";challenge=\"", *challenge, "\""}) : "";
  const std::string full_string =
      base::StrCat({algs_string, path_string, challenge_string});

  if (!headers) {
    headers = HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  }
  headers->AddHeader(kRegistrationHeader, full_string);

  return headers;
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, BasicValid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest,
    ExtraUnrecognizedAlgorithm) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "(ES256 bf512)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, NoHeader) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ChallengeFirst) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->SetHeader(
      kRegistrationHeader,
      "(RS256 ES256);challenge=\"challenge1\";path=\"first\"");

  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/first"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "challenge1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, NoSpaces) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->SetHeader(
      kRegistrationHeader,
      "(RS256 ES256);path=\"startsession\";challenge=\"challenge1\"");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "challenge1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, TwoRegistrations) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/first", "(ES256 RS256)", "c1");
  CreateHeaders("/second", "(ES256)", "challenge2", response_headers);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 2U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(), GURL("https://www.example.com/first"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "c1");

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/second"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "challenge2");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ValidInvalid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/first", "(ES256 RS256)", "c1");
  CreateHeaders("/second", "(es256)", "challenge2", response_headers);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(), GURL("https://www.example.com/first"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest,
     AddedInvalidNonsenseCharacters) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->AddHeader(kRegistrationHeader,
                              "(RS256);path=\"new\";challenge=\"test\";;=;");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest,
     AddedValidNonsenseCharacters) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->AddHeader(
      kRegistrationHeader,
      "(RS256);path=\"new\";challenge=\"test\";nonsense=\";';'\",OTHER");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(), GURL("https://www.example.com/new"));
  EXPECT_THAT(p1.supported_algos(), UnorderedElementsAre(RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "test");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, AlgAsString) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->AddHeader(kRegistrationHeader,
                              "(\"RS256\");path=\"new\";challenge=\"test\"");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, PathAsToken) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->AddHeader(kRegistrationHeader,
                              "(RS256);path=new;challenge=\"test\"");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ChallengeAsByteSequence) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->AddHeader(kRegistrationHeader,
                              "(RS256);path=\"new\";challenge=:Y29kZWQ=:");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ValidInvalidValid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/first", "(ES256 RS256)", "c1");
  CreateHeaders("/second", "(es256)", "challenge2", response_headers);
  CreateHeaders("/third", "(ES256)", "challenge3", response_headers);

  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 2U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(), GURL("https://www.example.com/first"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "c1");

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/third"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "challenge3");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ThreeRegistrations) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/startsession", "(ES256 RS256)", "c1");
  CreateHeaders("/new", "(ES256)", "coded", response_headers);
  CreateHeaders("/third", "(ES256)", "another", response_headers);

  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 3U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "c1");

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/new"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "coded");

  auto p3 = std::move(params[2]);
  EXPECT_EQ(p3.registration_endpoint(), GURL("https://www.example.com/third"));
  EXPECT_THAT(p3.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p3.challenge(), "another");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ThreeRegistrationsList) {
  GURL registration_request = GURL("https://www.example.com/registration");
  // Testing customized header
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/startsession", "(ES256 RS256)", "c1");
  response_headers->AddHeader(kRegistrationHeader,
                              "(ES256);path=\"new\";challenge=\"coded\", "
                              "(ES256);path=\"third\";challenge=\"another\"");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 3U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), "c1");

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/new"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "coded");

  auto p3 = std::move(params[2]);
  EXPECT_EQ(p3.registration_endpoint(), GURL("https://www.example.com/third"));
  EXPECT_THAT(p3.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p3.challenge(), "another");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, StartWithSlash) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/startsession", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, EscapeOnce) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/%2561", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(), GURL("https://www.example.com/%61"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, InvalidUrl) {
  GURL registration_request = GURL("https://[/");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("new", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 0U);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, HasUrlEncoded) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("test%2Fstart", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/test/start"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, FullUrl) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers = CreateHeaders(
      "https://accounts.example.com/startsession", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://accounts.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, SwapAlgo) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "(ES256 RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, OneAlgo) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "(RS256)", "c1");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  ASSERT_THAT(param.supported_algos(), UnorderedElementsAre(RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, InvalidParamIgnored) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  response_headers->SetHeader(
      kRegistrationHeader,
      "(RS256);path=\"first\";challenge=\"c1\";another=true");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/first"));
  ASSERT_THAT(param.supported_algos(), UnorderedElementsAre(RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), "c1");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, InvalidInputs) {
  struct Input {
    std::string request_url;
    std::optional<std::string> path;
    std::optional<std::string> algos;
    std::optional<std::string> challenge;
  };

  const Input kInvalidInputs[] = {
      // All invalid
      {"https://www.example.com/reg", "", "()", ""},
      // All missing
      {"https://www.example.com/reg", std::nullopt, std::nullopt, std::nullopt},
      // All valid different Url
      {"https://www.example.com/registration",
       "https://accounts.different.url/startsession", "(RS256)", "c1"},
      // Empty request Url
      {"", "start", "(RS256)", "c1"},
      // Empty algo
      {"https://www.example.com/reg", "start", "()", "c1"},
      // Missing algo
      {"https://www.example.com/reg", "start", std::nullopt, "c1"},
      // Missing registration
      {"https://www.example.com/reg", std::nullopt, "(ES256 RS256)", "c1"},
      // Missing challenge
      {"https://www.example.com/reg", "start", "(ES256 RS256)", std::nullopt},
      // Empty challenge
      {"https://www.example.com/reg", "start", "(ES256 RS256)", ""},
      // Challenge invalid utf8
      {"https://www.example.com/reg", "start", "(ES256 RS256)", "ab\xC0\x80"}};

  for (const auto& input : kInvalidInputs) {
    GURL registration_request = GURL(input.request_url);
    scoped_refptr<net::HttpResponseHeaders> response_headers =
        CreateHeaders(input.path, input.algos, input.challenge);
    SCOPED_TRACE(registration_request.spec() + "; " +
                 response_headers->raw_headers());
    std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
        DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
            registration_request, response_headers.get());
    EXPECT_TRUE(params.empty());
  }
}

}  // namespace
}  // namespace net

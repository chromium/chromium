// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/device_bound_session_registration_fetcher_param.h"

#include <optional>

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "crypto/signature_verifier.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "net/http/structured_headers.h"

namespace net {
namespace {

constexpr char kChallenge[] = ":Y2hhbGxlbmdl:";
constexpr char kDecodedChallenge[] = "challenge";
using crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
using crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
using ::testing::UnorderedElementsAre;

scoped_refptr<net::HttpResponseHeaders> CreateHeaders(
    std::optional<std::string> path,
    std::optional<std::string> algs,
    std::optional<std::string> challenge) {
  std::string path_string = path ? base::StrCat({"\"", *path, "\"", "; "}) : "";
  std::string algs_string =
      (algs && !algs->empty()) ? base::StrCat({*algs, "; "}) : "";
  std::string challenge_string =
      challenge ? base::StrCat({"challenge=", *challenge}) : "";
  std::string full_string =
      base::StrCat({path_string, algs_string, challenge_string});
  return HttpResponseHeaders::Builder({1, 1}, "200 OK")
      .AddHeader("Sec-Session-Registration", full_string)
      .Build();
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, BasicValid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest,
    ExtraUnrecognizedAlgorithm) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "es256;bf512", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, NoHeader) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ChallengeFirst) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";", "challenge=",
                                            kChallenge, "; ", "rs256;es256"}));
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, NoSpaces) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, TwoRegistrations) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;es256");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 2U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), kDecodedChallenge);

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/new"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "coded");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ValidInvalid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, AddedNonsenseCharacters) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;rs256;;=;");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, AlgAsString) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;\"rs256\"");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ChallengeAsString) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=\"Y29kZWQ=\";rs256");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ValidInvalidValid) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:");
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;es256");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 2U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), kDecodedChallenge);

  auto p2 = std::move(params[1]);
  EXPECT_EQ(p2.registration_endpoint(), GURL("https://www.example.com/new"));
  EXPECT_THAT(p2.supported_algos(), UnorderedElementsAre(ECDSA_SHA256));
  EXPECT_EQ(p2.challenge(), "coded");
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, ThreeRegistrations) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;es256");
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"third\";challenge=:YW5vdGhlcg==:;es256");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 3U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), kDecodedChallenge);

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
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("");
  response_headers->SetHeader("Sec-Session-Registration",
                              base::StrCat({"\"startsession\";challenge=",
                                            kChallenge, ";rs256;es256"}));
  response_headers->AddHeader("Sec-Session-Registration",
                              "\"new\";challenge=:Y29kZWQ=:;es256, "
                              "\"third\";challenge=:YW5vdGhlcg==:;es256");
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 3U);
  auto p1 = std::move(params[0]);
  EXPECT_EQ(p1.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(p1.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(p1.challenge(), kDecodedChallenge);

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
      CreateHeaders("/startsession", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, EscapeOnce) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("/%2561", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(), GURL("https://www.example.com/%61"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, InvalidUrl) {
  GURL registration_request = GURL("https://[/");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("[", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 0U);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, HasUrlEncoded) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("test%2Fstart", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/test/start"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, FullUrl) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers = CreateHeaders(
      "https://accounts.example.com/startsession", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://accounts.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, SwapAlgo) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "es256;rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  EXPECT_THAT(param.supported_algos(),
              UnorderedElementsAre(ECDSA_SHA256, RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, OneAlgo) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "rs256", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  ASSERT_THAT(param.supported_algos(), UnorderedElementsAre(RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
}

TEST(DeviceBoundSessionRegistrationFetcherParamTest, AddedParameter) {
  GURL registration_request = GURL("https://www.example.com/registration");
  scoped_refptr<net::HttpResponseHeaders> response_headers =
      CreateHeaders("startsession", "rs256;lolcat", kChallenge);
  std::vector<DeviceBoundSessionRegistrationFetcherParam> params =
      DeviceBoundSessionRegistrationFetcherParam::CreateIfValid(
          registration_request, response_headers.get());
  ASSERT_EQ(params.size(), 1U);
  auto param = std::move(params[0]);
  EXPECT_EQ(param.registration_endpoint(),
            GURL("https://www.example.com/startsession"));
  ASSERT_THAT(param.supported_algos(), UnorderedElementsAre(RSA_PKCS1_SHA256));
  EXPECT_EQ(param.challenge(), kDecodedChallenge);
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
      {"https://www.example.com/reg", "", "", ""},
      // All missing
      {"https://www.example.com/reg", std::nullopt, std::nullopt, std::nullopt},
      // All valid different Url
      {"https://www.example.com/registration",
       "https://accounts.different.url/startsession", "rs256", kChallenge},
      // Empty request Url
      {"", "start", "rs256", kChallenge},
      // Empty algo
      {"https://www.example.com/reg", "start", "", kChallenge},
      // Missing algo
      {"https://www.example.com/reg", "start", std::nullopt, kChallenge},
      // Missing registration
      {"https://www.example.com/reg", std::nullopt, "es256;rs256", kChallenge},
      // Missing challenge
      {"https://www.example.com/reg", "start", "es256;rs256", std::nullopt},
      // Empty challenge
      {"https://www.example.com/reg", "start", "es256;rs256", ""},
      // Challenge invalid utf8
      {"https://www.example.com/reg", "start", "es256;rs256", "ab\xC0\x80"}};

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

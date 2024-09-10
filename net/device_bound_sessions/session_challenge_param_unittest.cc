// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_challenge_param.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kSessionChallengeHeaderForTest[] = "Sec-Session-Challenge";
constexpr char kSessionIdKey[] = "id";
constexpr char kSampleSessionId[] = "session_id";
constexpr char kSampleChallenge[] = "challenge";
constexpr char kTestUrl[] = "https://www.example.com/refresh";

std::string CreateHeaderStringForTest(std::optional<std::string> session_id,
                                      std::string challenge) {
  if (session_id.has_value()) {
    return base::StringPrintf("\"%s\";%s=\"%s\"", challenge.c_str(),
                              kSessionIdKey, (*session_id).c_str());
  } else {
    return base::StringPrintf("\"%s\"", challenge.c_str());
  }
}

TEST(SessionChallengeParamTest, ValidBareChallenge) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(
      CreateHeaderStringForTest(std::nullopt, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, ValidSessionAndChallenge) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, InvalidURL) {
  const GURL url("invalid.url");
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, NoHeader) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptyHeader) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  builder.AddHeader(kSessionChallengeHeaderForTest, "");
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptySessionId) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(CreateHeaderStringForTest("", kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, EmptyChallenge) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(CreateHeaderStringForTest(kSampleSessionId, ""));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, NoQuotes) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string = base::StringPrintf(
      "%s;%s=\"%s\"", kSampleChallenge, kSessionIdKey, kSampleSessionId);
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, InvalidNonsenseCharacters) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string =
      base::StringPrintf("\"%s\"; %s=\"%s\";;=;OTHER", kSampleChallenge,
                         kSessionIdKey, kSampleSessionId);
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ExtraSymbol) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string =
      base::StringPrintf("\"%s\"; %s=\"%s\";cache", kSampleChallenge,
                         kSessionIdKey, kSampleSessionId);
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, ExtraParameters) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string =
      base::StringPrintf("\"%s\"; %s=\"%s\";cache;key=value;k=v",
                         kSampleChallenge, kSessionIdKey, kSampleSessionId);
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, InnerListParameter) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  builder.AddHeader(kSessionChallengeHeaderForTest,
                    "(\"challenge\";id=\"id\"), (\"challenge1\" \"id1\")");
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, SessionChallengeAsByteSequence) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string = base::StringPrintf(
      "\"%s\"; %s=%s", kSampleChallenge, kSessionIdKey, ":Y29kZWQ=:");
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, BareChallengeAsByteSequence) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  builder.AddHeader(kSessionChallengeHeaderForTest, ":Y29kZWQ=:");
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ValidTwoSessionChallenges) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string1(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string1);

  std::string session_id2("session_id2");
  std::string challenge2("nonce2");
  std::string header_string2(
      CreateHeaderStringForTest(session_id2, challenge2));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string2);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_EQ(params[1].session_id(), session_id2);
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidTwoBareChallenges) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string1(
      CreateHeaderStringForTest(std::nullopt, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string1);

  std::string challenge2("nonce2");
  std::string header_string2(
      CreateHeaderStringForTest(std::nullopt, challenge2));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string2);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_FALSE(params[1].session_id());
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidMixedChallenges) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string challenge("new");
  std::string header_string1(
      CreateHeaderStringForTest(std::nullopt, challenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string1);
  std::string header_string2(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string2);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), challenge);

  EXPECT_EQ(params[1].session_id(), kSampleSessionId);
  EXPECT_EQ(params[1].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, MixedHeaderParameterFirst) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string1(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string1);
  std::string challenge("new");
  std::string header_string2(
      CreateHeaderStringForTest(std::nullopt, challenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string2);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_FALSE(params[1].session_id());
  EXPECT_EQ(params[1].challenge(), challenge);
}

TEST(SessionChallengeParamTest, TwoChallengesInOneHeader) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string1(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));

  std::string session_id2("session_id2");
  std::string challenge2("nonce2");
  std::string header_string2(
      CreateHeaderStringForTest(session_id2, challenge2));
  std::string combined_header = base::StringPrintf(
      "%s,%s", header_string1.c_str(), header_string2.c_str());
  builder.AddHeader(kSessionChallengeHeaderForTest, combined_header);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_EQ(params[1].session_id(), session_id2);
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidInvalid) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  builder.AddHeader(kSessionChallengeHeaderForTest, ";;OTHER");
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptyHeaderValidHeader) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  builder.AddHeader(kSessionChallengeHeaderForTest, "");
  std::string header_string(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ThreeChallengesInTwoHeaders) {
  const GURL url(kTestUrl);
  net::HttpResponseHeaders::Builder builder =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK");
  std::string header_string1(
      CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge));

  std::string session_id2("session_id2");
  std::string challenge2("nonce2");
  std::string header_string2(
      CreateHeaderStringForTest(session_id2, challenge2));
  std::string combined_header = base::StringPrintf(
      "%s,%s", header_string1.c_str(), header_string2.c_str());
  builder.AddHeader(kSessionChallengeHeaderForTest, combined_header);

  std::string session_id3("session_id3");
  std::string challenge3("nonce3");
  std::string header_string3(
      CreateHeaderStringForTest(session_id3, challenge3));
  builder.AddHeader(kSessionChallengeHeaderForTest, header_string3);
  scoped_refptr<net::HttpResponseHeaders> headers = builder.Build();
  std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 3U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_EQ(params[1].session_id(), session_id2);
  EXPECT_EQ(params[1].challenge(), challenge2);

  EXPECT_EQ(params[2].session_id(), session_id3);
  EXPECT_EQ(params[2].challenge(), challenge3);
}

}  // namespace
}  // namespace net::device_bound_sessions

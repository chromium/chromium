// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_challenge_param.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

constexpr char kSessionChallengeHeaderForTest[] = "Sec-Session-Challenge";
constexpr char kSessionIdKey[] = "id";
constexpr char kTestUrl[] = "https://www.example.com/refresh";
constexpr base::cstring_view kSampleSessionId("session_id");
constexpr base::cstring_view kSampleChallenge("challenge");

std::string CreateHeaderStringForTest(
    std::optional<base::cstring_view> session_id,
    base::cstring_view challenge) {
  if (session_id.has_value()) {
    return base::StringPrintf(R"("%s";%s="%s")", challenge.c_str(),
                              kSessionIdKey, session_id->c_str());
  }
  return base::StringPrintf(R"("%s")", challenge.c_str());
}

TEST(SessionChallengeParamTest, ValidBareChallenge) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(std::nullopt, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, ValidSessionAndChallenge) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, InvalidURL) {
  const GURL url("invalid.url");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, NoHeader) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK").Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptyHeader) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest, "")
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptySessionId) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view empty_session_id{""};
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(empty_session_id, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, EmptyChallenge) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(kSampleSessionId, ""))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, NoQuotes) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     base::StringPrintf(R"(%s;%s="%s")", kSampleChallenge,
                                        kSessionIdKey, kSampleSessionId))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, InvalidNonsenseCharacters) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              base::StringPrintf(R"("%s"; %s="%s";;=;OTHER)", kSampleChallenge,
                                 kSessionIdKey, kSampleSessionId))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ExtraSymbol) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              base::StringPrintf(R"("%s"; %s="%s";cache)", kSampleChallenge,
                                 kSessionIdKey, kSampleSessionId))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, ExtraParameters) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     base::StringPrintf(R"("%s"; %s="%s";cache;key=value;k=v)",
                                        kSampleChallenge, kSessionIdKey,
                                        kSampleSessionId))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_EQ(params.size(), 1U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, InnerListParameter) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     R"(("challenge";id="id"), ("challenge1" "id1"))")
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, SessionChallengeAsByteSequence) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     base::StringPrintf(R"("%s"; %s=%s)", kSampleChallenge,
                                        kSessionIdKey, ":Y29kZWQ=:"))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, BareChallengeAsByteSequence) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest, ":Y29kZWQ=:")
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());
  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ValidTwoSessionChallenges) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view session_id2("session_id2");
  static constexpr base::cstring_view challenge2("nonce2");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(session_id2, challenge2))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_EQ(params[1].session_id(), session_id2);
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidTwoBareChallenges) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view challenge2("nonce2");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(std::nullopt, kSampleChallenge))
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(std::nullopt, challenge2))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_FALSE(params[1].session_id());
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidMixedChallenges) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view challenge("new");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(std::nullopt, challenge))
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_FALSE(params[0].session_id());
  EXPECT_EQ(params[0].challenge(), challenge);

  EXPECT_EQ(params[1].session_id(), kSampleSessionId);
  EXPECT_EQ(params[1].challenge(), kSampleChallenge);
}

TEST(SessionChallengeParamTest, MixedHeaderParameterFirst) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view challenge("new");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(std::nullopt, challenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_FALSE(params[1].session_id());
  EXPECT_EQ(params[1].challenge(), challenge);
}

TEST(SessionChallengeParamTest, TwoChallengesInOneHeader) {
  const GURL url(kTestUrl);
  static constexpr base::cstring_view session_id2("session_id2");
  static constexpr base::cstring_view challenge2("nonce2");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              base::StrCat(
                  {CreateHeaderStringForTest(kSampleSessionId,
                                             kSampleChallenge),
                   ",", CreateHeaderStringForTest(session_id2, challenge2)}))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_EQ(params.size(), 2U);
  EXPECT_EQ(params[0].session_id(), kSampleSessionId);
  EXPECT_EQ(params[0].challenge(), kSampleChallenge);

  EXPECT_EQ(params[1].session_id(), session_id2);
  EXPECT_EQ(params[1].challenge(), challenge2);
}

TEST(SessionChallengeParamTest, ValidInvalid) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .AddHeader(kSessionChallengeHeaderForTest, ";;OTHER")
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, EmptyHeaderValidHeader) {
  const GURL url(kTestUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(kSessionChallengeHeaderForTest, "")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              CreateHeaderStringForTest(kSampleSessionId, kSampleChallenge))
          .Build();
  const std::vector<SessionChallengeParam> params =
      SessionChallengeParam::CreateIfValid(url, headers.get());

  ASSERT_TRUE(params.empty());
}

TEST(SessionChallengeParamTest, ThreeChallengesInTwoHeaders) {
  GURL url(kTestUrl);
  static constexpr base::cstring_view session_id2("session_id2");
  static constexpr base::cstring_view challenge2("nonce2");
  static constexpr base::cstring_view session_id3("session_id3");
  static constexpr base::cstring_view challenge3("nonce3");
  scoped_refptr<net::HttpResponseHeaders> headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(
              kSessionChallengeHeaderForTest,
              base::StrCat(
                  {CreateHeaderStringForTest(kSampleSessionId,
                                             kSampleChallenge),
                   ", ", CreateHeaderStringForTest(session_id2, challenge2)}))
          .AddHeader(kSessionChallengeHeaderForTest,
                     CreateHeaderStringForTest(session_id3, challenge3))
          .Build();
  const std::vector<SessionChallengeParam> params =
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

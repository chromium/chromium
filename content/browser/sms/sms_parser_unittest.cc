// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include "base/optional.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

url::Origin ParseOrigin(const std::string& message) {
  base::Optional<SmsParser::Result> result = SmsParser::Parse(message);
  return result->origin;
}

std::string ParseOTP(const std::string& message) {
  base::Optional<SmsParser::Result> result = SmsParser::Parse(message);
  return result->one_time_code;
}

}  // namespace

TEST(SmsParserTest, NoToken) {
  ASSERT_FALSE(SmsParser::Parse("foo"));
}

TEST(SmsParserTest, WithTokenInvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("For: foo"));
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("To:https://example.com"));
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("For: //example.com"));
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("For: ftp://example.com"));
}

TEST(SmsParserTest, HttpScheme) {
  ASSERT_FALSE(SmsParser::Parse("For: http://example.com"));
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("For: mailto:goto@chromium.org"));
}

TEST(SmsParserTest, MissingOneTimeCodeParameter) {
  ASSERT_FALSE(SmsParser::Parse("For: https://example.com"));
}

TEST(SmsParserTest, Basic) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("For: https://example.com?otp=123"));
}

TEST(SmsParserTest, Realistic) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("<#> Your OTP is 1234ABC.\nFor: "
                        "https://example.com?otp=123&s3LhKBB0M33"));
}

TEST(SmsParserTest, OneTimeCode) {
  auto result = SmsParser::Parse("For: https://example.com?otp=123");
  ASSERT_EQ("123", (*result).one_time_code);
}

TEST(SmsParserTest, LocalhostForDevelopment) {
  ASSERT_EQ(url::Origin::Create(GURL("http://localhost:8080")),
            ParseOrigin("For: http://localhost:8080?otp=123"));
  ASSERT_EQ(url::Origin::Create(GURL("http://localhost:80")),
            ParseOrigin("For: http://localhost:80?otp=123"));
  ASSERT_EQ(url::Origin::Create(GURL("http://localhost")),
            ParseOrigin("For: http://localhost?otp=123"));
  ASSERT_FALSE(SmsParser::Parse("For: localhost"));
}

TEST(SmsParserTest, Paths) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("For: https://example.com/foobar?otp=123"));
}

TEST(SmsParserTest, Message) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\nFor: https://example.com?otp=123"));
}

TEST(SmsParserTest, Whitespace) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\nFor: https://example.com?otp=123 "));
}

TEST(SmsParserTest, Newlines) {
  ASSERT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\nFor: https://example.com?otp=123\n"));
}

TEST(SmsParserTest, TwoTokens) {
  ASSERT_EQ(url::Origin::Create(GURL("https://b.com")),
            ParseOrigin("For: https://a.com For: https://b.com?otp=123"));
}

TEST(SmsParserTest, DifferentPorts) {
  ASSERT_NE(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://a.com:8443/?otp=123"));
  ASSERT_NE(url::Origin::Create(GURL("https://a.com:443")),
            ParseOrigin("For: https://a.com:8443/?otp=123"));
}

TEST(SmsParserTest, ImplicitPort) {
  ASSERT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://a.com:443/?otp=123"));
  ASSERT_NE(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://a.com:8443/?otp=123"));
}

TEST(SmsParserTest, Redirector) {
  ASSERT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://a.com/redirect?otp=123&https://b.com"));
  ASSERT_EQ(
      url::Origin::Create(GURL("https://a.com")),
      ParseOrigin("For: https://a.com/redirect?&otp=123&https:%2f%2fb.com"));
  ASSERT_EQ(
      url::Origin::Create(GURL("https://a.com")),
      ParseOrigin("For: https://a.com/redirect?otp=123#https:%2f%2fb.com"));
}

TEST(SmsParserTest, UsernameAndPassword) {
  ASSERT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://b.com@a.com/?otp=123"));
  ASSERT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://b.com:c.com@a.com/?otp=123"));
  ASSERT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("For: https://b.com:noodle@a.com:443/?otp=123"));
}

TEST(SmsParserTest, HarmlessOriginsButInvalid) {
  ASSERT_FALSE(SmsParser::Parse("For: data://123"));
}

TEST(SmsParserTest, AppHash) {
  ASSERT_EQ(
      url::Origin::Create(GURL("https://example.com")),
      ParseOrigin(
          "<#> Hello World\nFor: https://example.com?otp=123&s3LhKBB0M33"));
}

TEST(SmsParserTest, OneTimeCodeCharRanges) {
  ASSERT_EQ("cannot-contain-hashes",
            ParseOTP("For: https://example.com?otp=cannot-contain-hashes#yes"));
  ASSERT_EQ(
      "can-contain-numbers-like-123",
      ParseOTP("For: https://example.com?otp=can-contain-numbers-like-123"));
  ASSERT_EQ(
      "can-contain-utf8-like-🤷",
      ParseOTP("For: https://example.com?otp=can-contain-utf8-like-🤷"));
  ASSERT_EQ(
      "can-contain-chars-like-*^$@",
      ParseOTP("For: https://example.com?otp=can-contain-chars-like-*^$@"));
  ASSERT_EQ(
      "human-readable-words-like-sillyface",
      ParseOTP(
          "For: https://example.com?otp=human-readable-words-like-sillyface"));
  ASSERT_EQ(
      "works-with-more-params",
      ParseOTP("For: https://example.com?otp=works-with-more-params&foo=bar"));
  ASSERT_EQ(
      "works-with-more-params",
      ParseOTP("For: https://example.com?foo=bar&otp=works-with-more-params"));
  ASSERT_EQ(
      "can-it-be-super-lengthy-like-a-lot",
      ParseOTP(
          "For: https://example.com?otp=can-it-be-super-lengthy-like-a-lot"));
}

}  // namespace content

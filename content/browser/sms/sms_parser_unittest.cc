// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_parser.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

url::Origin ParseOrigin(const std::string& message) {
  SmsParser::Result result = SmsParser::Parse(message);
  if (!result.IsValid())
    return url::Origin();
  return result.top_origin;
}

std::string ParseOTP(const std::string& message) {
  SmsParser::Result result = SmsParser::Parse(message);
  if (!result.IsValid())
    return "";
  return result.one_time_code;
}

}  // namespace

TEST(SmsParserTest, NoToken) {
  ASSERT_FALSE(SmsParser::Parse("foo").IsValid());
}

TEST(SmsParserTest, WithTokenInvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("@foo").IsValid());
}

TEST(SmsParserTest, NoSpace) {
  ASSERT_FALSE(SmsParser::Parse("@example.com#12345").IsValid());
}

TEST(SmsParserTest, MultipleSpace) {
  ASSERT_FALSE(SmsParser::Parse("@example.com  #12345").IsValid());
}

TEST(SmsParserTest, WhiteSpaceThatIsNotSpace) {
  ASSERT_FALSE(SmsParser::Parse("@example.com\t#12345").IsValid());
}

TEST(SmsParserTest, WordInBetween) {
  ASSERT_FALSE(SmsParser::Parse("@example.com random #12345").IsValid());
}

TEST(SmsParserTest, InvalidUrl) {
  ASSERT_FALSE(SmsParser::Parse("@//example.com #123").IsValid());
}

TEST(SmsParserTest, FtpScheme) {
  ASSERT_FALSE(SmsParser::Parse("@ftp://example.com #123").IsValid());
}

TEST(SmsParserTest, Mailto) {
  ASSERT_FALSE(SmsParser::Parse("@mailto:goto@chromium.org #123").IsValid());
}

TEST(SmsParserTest, MissingOneTimeCodeParameter) {
  ASSERT_FALSE(SmsParser::Parse("@example.com").IsValid());
}

TEST(SmsParserTest, Basic) {
  auto result = SmsParser::Parse("@example.com #12345");

  ASSERT_TRUE(result.IsValid());
  EXPECT_EQ("12345", result.one_time_code);
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            result.top_origin);
}

TEST(SmsParserTest, Realistic) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("<#> Your OTP is 1234ABC.\n@example.com #12345"));
}

TEST(SmsParserTest, OneTimeCode) {
  EXPECT_EQ("123", ParseOTP("@example.com #123"));
}

TEST(SmsParserTest, LocalhostForDevelopment) {
  EXPECT_EQ(url::Origin::Create(GURL("http://localhost")),
            ParseOrigin("@localhost #123"));
  ASSERT_FALSE(SmsParser::Parse("@localhost:8080 #123").IsValid());
  ASSERT_FALSE(SmsParser::Parse("@localhost").IsValid());
}

TEST(SmsParserTest, Paths) {
  ASSERT_FALSE(SmsParser::Parse("@example.com/foobar #123").IsValid());
}

TEST(SmsParserTest, Message) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123"));
}

TEST(SmsParserTest, Whitespace) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123 "));
}

TEST(SmsParserTest, Dashes) {
  EXPECT_EQ(url::Origin::Create(GURL("https://web-otp-example.com")),
            ParseOrigin("@web-otp-example.com #123"));
}

TEST(SmsParserTest, CapitalLetters) {
  EXPECT_EQ(url::Origin::Create(GURL("https://can-contain-CAPITAL.com")),
            ParseOrigin("@can-contain-CAPITAL.com #123"));
}

TEST(SmsParserTest, Numbers) {
  EXPECT_EQ(url::Origin::Create(GURL("https://can-contain-number-9870.com")),
            ParseOrigin("@can-contain-number-9870.com #123"));
}

TEST(SmsParserTest, ForbiddenCharacters) {
  // TODO(majidvp): Domains with unicode characters are valid.
  // See: https://url.spec.whatwg.org/#concept-domain-to-ascii
  // EXPECT_EQ(url::Origin::Create(GURL("can-contain-unicode-like-חומוס.com")),
  //            ParseOrigin("@can-contain-unicode-like-חומוס.com #123"));

  // Forbidden codepoints https://url.spec.whatwg.org/#forbidden-host-code-point
  const char forbidden_chars[] = {'\x00' /* null */,
                                  '\x09' /* TAB */,
                                  '\x0A' /* LF */,
                                  '\x0D' /* CR */,
                                  ' ',
                                  '#',
                                  '%',
                                  '/',
                                  ':',
                                  '<',
                                  '>',
                                  '?',
                                  '@',
                                  '[',
                                  '\\',
                                  ']',
                                  '^'};
  for (char c : forbidden_chars) {
    ASSERT_FALSE(
        SmsParser::Parse(base::StringPrintf("@cannot-contain-%c #123456", c))
            .IsValid());
  }
}

TEST(SmsParserTest, Newlines) {
  EXPECT_EQ(url::Origin::Create(GURL("https://example.com")),
            ParseOrigin("hello world\n@example.com #123\n"));
}

TEST(SmsParserTest, TwoTokens) {
  EXPECT_EQ(url::Origin::Create(GURL("https://b.com")),
            ParseOrigin("@a.com @b.com #123"));
  EXPECT_EQ(url::Origin::Create(GURL("https://a.com")),
            ParseOrigin("@a.com #123 @b.com"));
}

TEST(SmsParserTest, Ports) {
  ASSERT_FALSE(SmsParser::Parse("@a.com:8443 #123").IsValid());
}

TEST(SmsParserTest, Username) {
  ASSERT_FALSE(SmsParser::Parse("@username@a.com #123").IsValid());
}

TEST(SmsParserTest, QueryParams) {
  ASSERT_FALSE(SmsParser::Parse("@a.com/?foo=123 #123").IsValid());
}

TEST(SmsParserTest, HarmlessOriginsButInvalid) {
  ASSERT_FALSE(SmsParser::Parse("@data://123").IsValid());
}

TEST(SmsParserTest, AppHash) {
  EXPECT_EQ(
      url::Origin::Create(GURL("https://example.com")),
      ParseOrigin("<#> Hello World\nApp Hash: s3LhKBB0M33\n@example.com #123"));
}

TEST(SmsParserTest, OneTimeCodeCharRanges) {
  EXPECT_EQ("cannot-contain-hashes",
            ParseOTP("@example.com #cannot-contain-hashes#yes"));
  EXPECT_EQ("can-contain-numbers-like-123",
            ParseOTP("@example.com #can-contain-numbers-like-123"));
  EXPECT_EQ("can-contain-utf8-like-🤷",
            ParseOTP("@example.com #can-contain-utf8-like-🤷"));
  EXPECT_EQ("can-contain-chars-like-*^$@",
            ParseOTP("@example.com #can-contain-chars-like-*^$@"));
  EXPECT_EQ("human-readable-words-like-sillyface",
            ParseOTP("@example.com #human-readable-words-like-sillyface"));
  EXPECT_EQ("can-it-be-super-lengthy-like-a-lot",
            ParseOTP("@example.com #can-it-be-super-lengthy-like-a-lot"));
  EXPECT_EQ("1", ParseOTP("@example.com #1 can be short"));
  EXPECT_EQ("otp", ParseOTP("@example.com #otp with space"));
  EXPECT_EQ("otp", ParseOTP("@example.com #otp\twith with tab"));
}

TEST(SmsParserTest, EmbeddedIFrameAfterSingleSpace) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123 @embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_EQ(url::Origin::Create(GURL("https://embedded.com")),
            result.embedded_origin);
  EXPECT_EQ("123", result.one_time_code);
}

TEST(SmsParserTest, EmbeddedIFrameAfterMultipleSpaces) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123  @embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_TRUE(result.embedded_origin.opaque());
  EXPECT_EQ("123", result.one_time_code);
}

TEST(SmsParserTest, EmbeddedIFrameAfterTab) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123\t@embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_TRUE(result.embedded_origin.opaque());
  EXPECT_EQ("123", result.one_time_code);
}

TEST(SmsParserTest, EmbeddedIFrameAfterNewLine) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123\n@embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_TRUE(result.embedded_origin.opaque());
  EXPECT_EQ("123", result.one_time_code);
}

TEST(SmsParserTest, EmbeddedIFrameAfterNonWhiteSpace) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123@embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_TRUE(result.embedded_origin.opaque());
  EXPECT_EQ("123@embedded.com", result.one_time_code);
}

TEST(SmsParserTest, OnlyFirstNonTopDomainConsidered) {
  SmsParser::Result result =
      SmsParser::Parse("@top.com #123 @embedded.com @nested.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_EQ(url::Origin::Create(GURL("https://embedded.com")),
            result.embedded_origin);
  EXPECT_EQ("123", result.one_time_code);
}

TEST(SmsParserTest, EmbeddedIFrameWithIncorrectToken) {
  SmsParser::Result result = SmsParser::Parse("@top.com #123 %embedded.com");
  EXPECT_EQ(url::Origin::Create(GURL("https://top.com")), result.top_origin);
  EXPECT_TRUE(result.embedded_origin.opaque());
  EXPECT_EQ("123", result.one_time_code);
}

}  // namespace content

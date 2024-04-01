// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_extension_parser.h"

#include <string>

#include "net/websockets/websocket_extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(WebSocketExtensionParserTest, ParseEmpty) {
  WebSocketExtensionParser parser;
  EXPECT_FALSE(parser.Parse("", 0));

  EXPECT_EQ(0U, parser.extensions().size());
}

TEST(WebSocketExtensionParserTest, ParseSimple) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");

  EXPECT_TRUE(parser.Parse("foo"));

  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

TEST(WebSocketExtensionParserTest, ParseMoreThanOnce) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");

  EXPECT_TRUE(parser.Parse("foo"));
  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));

  EXPECT_FALSE(parser.Parse(""));
  EXPECT_EQ(0U, parser.extensions().size());

  EXPECT_TRUE(parser.Parse("foo"));
  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithOneParamWithoutValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar"));

  EXPECT_TRUE(parser.Parse("\tfoo ; bar"));

  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithOneParamWithValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));

  EXPECT_TRUE(parser.Parse("foo ; bar= baz\t"));

  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

TEST(WebSocketExtensionParserTest, ParseOneExtensionWithParams) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));
  expected.Add(WebSocketExtension::Parameter("hoge", "fuga"));

  EXPECT_TRUE(parser.Parse("foo ; bar= baz;\t \thoge\t\t=fuga"));

  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

TEST(WebSocketExtensionParserTest, ParseTwoExtensions) {
  WebSocketExtensionParser parser;

  WebSocketExtension expected0("foo");
  expected0.Add(WebSocketExtension::Parameter("alpha", "x"));

  WebSocketExtension expected1("bar");
  expected1.Add(WebSocketExtension::Parameter("beta", "y"));

  EXPECT_TRUE(parser.Parse(" foo ; alpha = x , bar ; beta = y "));

  ASSERT_EQ(2U, parser.extensions().size());

  EXPECT_TRUE(expected0.Equivalent(parser.extensions()[0]));
  EXPECT_TRUE(expected1.Equivalent(parser.extensions()[1]));
}

TEST(WebSocketExtensionParserTest, InvalidPatterns) {
  const char* const patterns[] = {
      ",",                    // just a comma
      " , ",                  // just a comma with surrounding spaces
      "foo,",                 // second extension is incomplete (empty)
      "foo , ",               // second extension is incomplete (space)
      "foo,;",                // second extension is incomplete (semicolon)
      "foo;, bar",            // first extension is incomplete
      "fo\ao",                // control in extension name
      "fo\x01o",              // control in extension name
      "fo<o",                 // separator in extension name
      "foo/",                 // separator in extension name
      ";bar",                 // empty extension name
      "foo bar",              // missing ';'
      "foo;",                 // extension parameter without name and value
      "foo; b\ar",            // control in parameter name
      "foo; b\x7fr",          // control in parameter name
      "foo; b[r",             // separator in parameter name
      "foo; ba:",             // separator in parameter name
      "foo; =baz",            // empty parameter name
      "foo; bar=",            // empty parameter value
      "foo; =",               // empty parameter name and value
      "foo; bar=b\x02z",      // control in parameter value
      "foo; bar=b@z",         // separator in parameter value
      "foo; bar=b\\z",        // separator in parameter value
      "foo; bar=b?z",         // separator in parameter value
      "\"foo\"",              // quoted extension name
      "foo; \"bar\"",         // quoted parameter name
      "foo; bar=\"\a2\"",     // control in quoted parameter value
      "foo; bar=\"b@z\"",     // separator in quoted parameter value
      "foo; bar=\"b\\\\z\"",  // separator in quoted parameter value
      "foo; bar=\"\"",        // quoted empty parameter value
      "foo; bar=\"baz",       // unterminated quoted string
      "foo; bar=\"baz \"",    // space in quoted string
      "foo; bar baz",         // missing '='
      "foo; bar - baz",  // '-' instead of '=' (note: "foo; bar-baz" is valid).
      "foo; bar=\r\nbaz",   // CRNL not followed by a space
      "foo; bar=\r\n baz",  // CRNL followed by a space
      "f\xFFpp",            // 8-bit character in extension name
      "foo; b\xFFr=baz"     // 8-bit character in parameter name
      "foo; bar=b\xFF"      // 8-bit character in parameter value
      "foo; bar=\"b\xFF\""  // 8-bit character in quoted parameter value
      "foo; bar=\"baz\\"    // ends with backslash
  };

  for (const auto* pattern : patterns) {
    WebSocketExtensionParser parser;
    EXPECT_FALSE(parser.Parse(pattern));
    EXPECT_EQ(0U, parser.extensions().size());
  }
}

TEST(WebSocketExtensionParserTest, QuotedParameterValue) {
  WebSocketExtensionParser parser;
  WebSocketExtension expected("foo");
  expected.Add(WebSocketExtension::Parameter("bar", "baz"));

  EXPECT_TRUE(parser.Parse("foo; bar = \"ba\\z\" "));

  ASSERT_EQ(1U, parser.extensions().size());
  EXPECT_TRUE(expected.Equivalent(parser.extensions()[0]));
}

// This is a regression test for crbug.com/647156
TEST(WebSocketExtensionParserTest, InvalidToken) {
  static constexpr char kInvalidInput[] = "\304;\304!*777\377=\377\254\377";
  WebSocketExtensionParser parser;
  EXPECT_FALSE(parser.Parse(kInvalidInput));
}

}  // namespace

}  // namespace net

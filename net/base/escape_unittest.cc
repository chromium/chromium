// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "net/base/escape.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

struct EscapeCase {
  const char* input;
  const char* output;
};

struct EscapeForHTMLCase {
  const char* input;
  const char* expected_output;
};

TEST(EscapeTest, EscapeTextForFormSubmission) {
  const EscapeCase escape_cases[] = {
    {"foo", "foo"},
    {"foo bar", "foo+bar"},
    {"foo++", "foo%2B%2B"}
  };
  for (const auto& escape_case : escape_cases) {
    EXPECT_EQ(escape_case.output,
              EscapeQueryParamValue(escape_case.input, true));
  }

  const EscapeCase escape_cases_no_plus[] = {
    {"foo", "foo"},
    {"foo bar", "foo%20bar"},
    {"foo++", "foo%2B%2B"}
  };
  for (const auto& escape_case : escape_cases_no_plus) {
    EXPECT_EQ(escape_case.output,
              EscapeQueryParamValue(escape_case.input, false));
  }

  // Test all the values in we're supposed to be escaping.
  const std::string no_escape(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!'()*-._~");
  for (int i = 0; i < 256; ++i) {
    std::string in;
    in.push_back(i);
    std::string out = EscapeQueryParamValue(in, true);
    if (0 == i) {
      EXPECT_EQ(out, std::string("%00"));
    } else if (32 == i) {
      // Spaces are plus escaped like web forms.
      EXPECT_EQ(out, std::string("+"));
    } else if (no_escape.find(in) == std::string::npos) {
      // Check %hex escaping
      std::string expected = base::StringPrintf("%%%02X", i);
      EXPECT_EQ(expected, out);
    } else {
      // No change for things in the no_escape list.
      EXPECT_EQ(out, in);
    }
  }
}

TEST(EscapeTest, EscapePath) {
  ASSERT_EQ(
    // Most of the character space we care about, un-escaped
    EscapePath(
      "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
      "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz"
      "{|}~\x7f\x80\xff"),
    // Escaped
    "%02%0A%1D%20!%22%23$%25&'()*+,-./0123456789%3A;"
    "%3C=%3E%3F@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
    "%7B%7C%7D~%7F%80%FF");
}

TEST(EscapeTest, EscapeUrlEncodedData) {
  ASSERT_EQ(
    // Most of the character space we care about, un-escaped
    EscapeUrlEncodedData(
      "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
      "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz"
      "{|}~\x7f\x80\xff", true),
    // Escaped
    "%02%0A%1D+!%22%23%24%25%26%27()*%2B,-./0123456789:%3B"
    "%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
    "%7B%7C%7D~%7F%80%FF");
}

TEST(EscapeTest, EscapeUrlEncodedDataSpace) {
  ASSERT_EQ(EscapeUrlEncodedData("a b", true), "a+b");
  ASSERT_EQ(EscapeUrlEncodedData("a b", false), "a%20b");
}

TEST(EscapeTest, EscapeForHTML) {
  const EscapeForHTMLCase tests[] = {
      {"hello", "hello"},
      {"<hello>", "&lt;hello&gt;"},
      {"don\'t mess with me", "don&#39;t mess with me"},
  };
  for (const auto& test : tests) {
    std::string result = EscapeForHTML(std::string(test.input));
    EXPECT_EQ(std::string(test.expected_output), result);
  }
}

TEST(EscapeTest, UnescapeForHTML) {
  const EscapeForHTMLCase tests[] = {
      {"", ""},
      {"&lt;hello&gt;", "<hello>"},
      {"don&#39;t mess with me", "don\'t mess with me"},
      {"&lt;&gt;&amp;&quot;&#39;", "<>&\"'"},
      {"& lt; &amp ; &; '", "& lt; &amp ; &; '"},
      {"&amp;", "&"},
      {"&quot;", "\""},
      {"&#39;", "'"},
      {"&lt;", "<"},
      {"&gt;", ">"},
      {"&amp; &", "& &"},
  };
  for (const auto& test : tests) {
    std::u16string result = UnescapeForHTML(base::ASCIIToUTF16(test.input));
    EXPECT_EQ(base::ASCIIToUTF16(test.expected_output), result);
  }
}

TEST(EscapeTest, EscapeExternalHandlerValue) {
  ASSERT_EQ(
      // Escaped
      "%02%0A%1D%20!%22#$%25&'()*+,-./0123456789:;"
      "%3C=%3E?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[%5C]%5E_%60abcdefghijklmnopqrstuvwxyz"
      "%7B%7C%7D~%7F%80%FF",
      // Most of the character space we care about, un-escaped
      EscapeExternalHandlerValue(
          "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
          "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "[\\]^_`abcdefghijklmnopqrstuvwxyz"
          "{|}~\x7f\x80\xff"));

  ASSERT_EQ(
      "!#$&'()*+,-./0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]_"
      "abcdefghijklmnopqrstuvwxyz~",
      EscapeExternalHandlerValue(
          "!#$&'()*+,-./0123456789:;=?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]_"
          "abcdefghijklmnopqrstuvwxyz~"));

  ASSERT_EQ("%258k", EscapeExternalHandlerValue("%8k"));
  ASSERT_EQ("a%25", EscapeExternalHandlerValue("a%"));
  ASSERT_EQ("%25a", EscapeExternalHandlerValue("%a"));
  ASSERT_EQ("a%258", EscapeExternalHandlerValue("a%8"));
  ASSERT_EQ("%ab", EscapeExternalHandlerValue("%ab"));
  ASSERT_EQ("%AB", EscapeExternalHandlerValue("%AB"));

  ASSERT_EQ("http://example.com/path/sub?q=a%7Cb%7Cc&q=1%7C2%7C3#ref%7C",
            EscapeExternalHandlerValue(
                "http://example.com/path/sub?q=a|b|c&q=1|2|3#ref|"));
  ASSERT_EQ("http://example.com/path/sub?q=a%7Cb%7Cc&q=1%7C2%7C3#ref%7C",
            EscapeExternalHandlerValue(
                "http://example.com/path/sub?q=a%7Cb%7Cc&q=1%7C2%7C3#ref%7C"));
  ASSERT_EQ("http://[2001:db8:0:1]:80",
            EscapeExternalHandlerValue("http://[2001:db8:0:1]:80"));
}

TEST(EscapeTest, EscapeNonASCII) {
  EXPECT_EQ("abc\n%2580%80", EscapeNonASCIIAndPercent("abc\n%80\x80"));
  EXPECT_EQ("abc\n%80%80", EscapeNonASCII("abc\n%80\x80"));
}

}  // namespace
}  // namespace net

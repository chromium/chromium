// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cookies/parsed_cookie.h"

#include <string>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(ParsedCookieTest, TestBasic) {
  ParsedCookie pc1("a=b");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_FALSE(pc1.IsSecure());
  EXPECT_FALSE(pc1.IsHttpOnly());
  EXPECT_FALSE(pc1.IsPartitioned());
  EXPECT_EQ("a", pc1.Name());
  EXPECT_EQ("b", pc1.Value());
  EXPECT_FALSE(pc1.HasPath());
  EXPECT_FALSE(pc1.HasDomain());
  EXPECT_FALSE(pc1.HasExpires());
  EXPECT_FALSE(pc1.HasMaxAge());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc1.SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_DEFAULT, pc1.Priority());

  ParsedCookie pc2(
      "c=d; secure; httponly; path=/foo; domain=bar.test; "
      "max-age=60; samesite=lax; priority=high; partitioned;");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_TRUE(pc2.IsSecure());
  EXPECT_TRUE(pc2.IsHttpOnly());
  EXPECT_TRUE(pc2.IsPartitioned());
  EXPECT_EQ("c", pc2.Name());
  EXPECT_EQ("d", pc2.Value());
  EXPECT_TRUE(pc2.HasPath());
  EXPECT_EQ("/foo", pc2.Path());
  EXPECT_TRUE(pc2.HasDomain());
  EXPECT_EQ("bar.test", pc2.Domain());
  EXPECT_FALSE(pc2.HasExpires());
  EXPECT_TRUE(pc2.HasMaxAge());
  EXPECT_EQ("60", pc2.MaxAge());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc2.SameSite());
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_HIGH, pc2.Priority());
}

TEST(ParsedCookieTest, TestEmpty) {
  const char* kTestCookieLines[]{"",    "     ", "=",     "=;",  " =;",
                                 "= ;", " = ;",  ";",     " ;",  " ; ",
                                 "\t",  "\t;",   "\t=\t", "\t=", "=\t"};

  for (const char* test : kTestCookieLines) {
    ParsedCookie pc(test);
    EXPECT_FALSE(pc.IsValid());
  }
}

TEST(ParsedCookieTest, TestSetEmptyNameValue) {
  CookieInclusionStatus status;
  ParsedCookie empty("", &status);
  EXPECT_FALSE(empty.IsValid());
  EXPECT_TRUE(status.HasExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_NO_COOKIE_CONTENT));
  EXPECT_FALSE(empty.SetValue(""));
  EXPECT_FALSE(empty.IsValid());

  ParsedCookie empty_value("name=");
  EXPECT_TRUE(empty_value.IsValid());
  EXPECT_EQ("name", empty_value.Name());
  EXPECT_FALSE(empty_value.SetName(""));
  EXPECT_EQ("name", empty_value.Name());
  EXPECT_TRUE(empty_value.IsValid());

  ParsedCookie empty_name("value");
  EXPECT_TRUE(empty_name.IsValid());
  EXPECT_EQ("value", empty_name.Value());
  EXPECT_FALSE(empty_name.SetValue(""));
  EXPECT_EQ("value", empty_name.Value());
  EXPECT_TRUE(empty_name.IsValid());
}

TEST(ParsedCookieTest, ParseValueStrings) {
  std::string valid_values[] = {
      "httpONLY", "1%7C1624663551161", "<K0<r<C_<G_<S0",
      "lastRequest=1624663552846&activeDays=%5B0%2C0", "si=8da88dce-5fee-4835"};
  for (const auto& value : valid_values) {
    EXPECT_EQ(ParsedCookie::ParseValueString(value), value);
    EXPECT_TRUE(ParsedCookie::ValueMatchesParsedValue(value));
  }

  std::string invalid_values[] = {
      "\nhttpONLYsecure",            // Newline char at start
      "httpONLY\nsecure",            // Newline char in middle
      "httpONLYsecure\n",            // Newline char at end
      "\r<K0<r<C_<G_<S0",            // Carriage return at start
      "<K0<r\r<C_<G_<S0",            // Carriage return in middle
      "<K0<r<C_<G_<S0\r",            // Carriage return at end
      ";lastRequest=1624663552846",  // Token separator at start
      "lastRequest=1624663552846; activeDays=%5B0%2C0",  // Token separator in
                                                         // middle
      std::string("\0abcdef", 7),                        // 0 byte at start
      std::string("abc\0def", 7),                        // 0 byte in middle
      std::string("abcdef\0", 7)};                       // 0 byte at end
  for (const auto& value : invalid_values) {
    EXPECT_NE(ParsedCookie::ParseValueString(value), value);
    EXPECT_FALSE(ParsedCookie::ValueMatchesParsedValue(value));
  }

  // Strings with leading whitespace should parse OK but
  // ValueMatchesParsedValue() should fail.
  std::string leading_whitespace_values[] = {
      " 1%7C1624663551161",   // Space at start
      "\t1%7C1624663551161",  // Tab at start
  };
  for (const auto& value : leading_whitespace_values) {
    EXPECT_TRUE(ParsedCookie::ParseValueString(value).length() ==
                value.length() - 1);
    EXPECT_FALSE(ParsedCookie::ValueMatchesParsedValue(value));
  }

  // Strings with trailing whitespace or the separator character should parse OK
  // but ValueMatchesParsedValue() should fail.
  std::string valid_values_with_trailing_chars[] = {
      "lastRequest=1624663552846 ",   // Space at end
      "lastRequest=1624663552846\t",  // Tab at end
      "lastRequest=1624663552846;",   // Token separator at end
  };
  const size_t valid_value_length =
      valid_values_with_trailing_chars[0].length() - 1;
  for (const auto& value : valid_values_with_trailing_chars) {
    EXPECT_TRUE(ParsedCookie::ParseValueString(value).length() ==
                valid_value_length);
    EXPECT_FALSE(ParsedCookie::ValueMatchesParsedValue(value));
  }

  // A valid value (truncated after the ';') but parses out to a substring.
  std::string value_with_separator_in_middle(
      "lastRequest=1624663552846; activeDays=%5B0%2C0");
  EXPECT_TRUE(
      ParsedCookie::ParseValueString(value_with_separator_in_middle).length() ==
      value_with_separator_in_middle.find(';'));
  EXPECT_FALSE(
      ParsedCookie::ValueMatchesParsedValue(value_with_separator_in_middle));
}

TEST(ParsedCookieTest, TestQuoted) {
  // These are some quoting cases which the major browsers all
  // handle differently.  I've tested Internet Explorer 6, Opera 9.6,
  // Firefox 3, and Safari Windows 3.2.1.  We originally tried to match
  // Firefox closely, however we now match Internet Explorer and Safari.
  const struct {
    const char* input;
    const char* expected;
  } kTests[] = {
      // Trailing whitespace after a quoted value.  The whitespace after
      // the quote is stripped in all browsers.
      {"\"zzz \"  ", "\"zzz \""},
      // Handling a quoted value with a ';', like FOO="zz;pp"  ;
      // IE and Safari: "zz;
      // Firefox and Opera: "zz;pp"
      {"\"zz;pp\" ;", "\"zz"},
      // Handling a value with multiple quoted parts, like FOO="zzz "   "ppp" ;
      // IE and Safari: "zzz "   "ppp";
      // Firefox: "zzz ";
      // Opera: <rejects cookie>
      {
          "\"zzz \"   \"ppp\" ",
          "\"zzz \"   \"ppp\"",
      },
      // A quote in a value that didn't start quoted.  like FOO=A"B ;
      // IE, Safari, and Firefox: A"B;
      // Opera: <rejects cookie>
      {
          "A\"B",
          "A\"B",
      }};

  for (const auto& test : kTests) {
    ParsedCookie pc(std::string("aBc=") + test.input +
                    " ; path=\"/\"  ; httponly ");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_FALSE(pc.IsSecure());
    EXPECT_TRUE(pc.IsHttpOnly());
    EXPECT_TRUE(pc.HasPath());
    EXPECT_EQ("aBc", pc.Name());
    EXPECT_EQ(test.expected, pc.Value());

    EXPECT_TRUE(pc.SetValue(pc.Value()));
    EXPECT_EQ(test.expected, pc.Value());

    // If a path was quoted, the path attribute keeps the quotes.  This will
    // make the cookie effectively useless, but path parameters aren't
    // supposed to be quoted.  Bug 1261605.
    EXPECT_EQ("\"/\"", pc.Path());
  }
}

TEST(ParsedCookieTest, TestNameless) {
  ParsedCookie pc("BLAHHH; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
}

TEST(ParsedCookieTest, TestAttributeCase) {
  ParsedCookie pc(
      "BLAH; Path=/; sECuRe; httpONLY; sAmESitE=LaX; pRIoRitY=hIgH; "
      "pARTitIoNeD;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_TRUE(pc.IsPartitioned());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAH", pc.Value());
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, pc.Priority());
  EXPECT_EQ(6U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TestDoubleQuotedNameless) {
  ParsedCookie pc("\"BLA\\\"HHH\"; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("\"BLA\\\"HHH\"", pc.Value());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuoteOffTheEnd) {
  ParsedCookie pc("a=\"B");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("\"B", pc.Value());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingName) {
  ParsedCookie pc("=ABC");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("ABC", pc.Value());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(0U, pc.NumberOfAttributes());

  // Ensure that a preceding equal sign is emitted in the cookie line.

  // Note that this goes against what's specified in RFC6265bis and differs from
  // how CanonicalCookie produces cookie lines. As currently written (draft 9),
  // the spec says that a cookie with an empty name should not prepend an '='
  // character when writing out the cookie line, but in the case where the value
  // already contains an equal sign the cookie line will be parsed incorrectly
  // on the receiving end. ParsedCookie.ToCookieLine is only used by the
  // extensions API to feed modified cookies into a network request for
  // reparsing, though, so here it's more important that the values always
  // deserialize correctly than conform to the spec
  ParsedCookie pc2("=ABC");
  EXPECT_EQ("=ABC", pc2.ToCookieLine());
  EXPECT_TRUE(pc2.SetValue("param=value"));
  EXPECT_EQ("=param=value", pc2.ToCookieLine());
  ParsedCookie pc3("=param=value");
  EXPECT_EQ("", pc3.Name());
  EXPECT_EQ("param=value", pc3.Value());
  EXPECT_EQ("=param=value", pc3.ToCookieLine());
}

TEST(ParsedCookieTest, MissingValue) {
  ParsedCookie pc("ABC=;  path = /wee");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ABC", pc.Name());
  EXPECT_EQ("", pc.Value());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/wee", pc.Path());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(1U, pc.NumberOfAttributes());

  // Ensure that a trailing equal sign is emitted in the cookie line
  ParsedCookie pc2("ABC=");
  EXPECT_EQ("ABC=", pc2.ToCookieLine());
}

TEST(ParsedCookieTest, Whitespace) {
  ParsedCookie pc("  A  = BC  ;secure;;;   samesite = lax     ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_FALSE(pc.IsHttpOnly());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  // We parse anything between ; as attributes, so we end up with two
  // attributes with an empty string name and value.
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}
TEST(ParsedCookieTest, MultipleEquals) {
  ParsedCookie pc("  A=== BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("== BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuotedTrailingWhitespace) {
  ParsedCookie pc(
      "ANCUUID=\"zohNumRKgI0oxyhSsV3Z7D\"  ; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  // Stripping whitespace after the quotes matches all other major browsers.
  EXPECT_EQ("\"zohNumRKgI0oxyhSsV3Z7D\"", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TrailingWhitespace) {
  ParsedCookie pc(
      "ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  EXPECT_EQ("zohNumRKgI0oxyhSsV3Z7D", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, LotsOfPairs) {
  for (int i = 1; i < 100; i++) {
    std::string blankpairs;
    blankpairs.resize(i, ';');

    ParsedCookie c("a=b;" + blankpairs + "secure");
    EXPECT_EQ("a", c.Name());
    EXPECT_EQ("b", c.Value());
    EXPECT_TRUE(c.IsValid());
    EXPECT_TRUE(c.IsSecure());
  }
}

TEST(ParsedCookieTest, EnforceSizeConstraints) {
  CookieInclusionStatus status;

  // Create maximum size and one-less-than-maximum size name and value
  // strings for testing.
  std::string max_name(ParsedCookie::kMaxCookieNamePlusValueSize, 'a');
  std::string max_value(ParsedCookie::kMaxCookieNamePlusValueSize, 'b');
  std::string almost_max_name = max_name.substr(1, std::string::npos);
  std::string almost_max_value = max_value.substr(1, std::string::npos);

  // Test name + value size limits enforced by the constructor.
  ParsedCookie pc1(max_name + "=");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ(max_name, pc1.Name());

  ParsedCookie pc2(max_name + "=; path=/foo;");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ(max_name, pc2.Name());

  ParsedCookie pc3(max_name + "X=", &status);
  EXPECT_FALSE(pc3.IsValid());
  EXPECT_TRUE(status.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE));

  ParsedCookie pc4("=" + max_value);
  EXPECT_TRUE(pc4.IsValid());
  EXPECT_EQ(max_value, pc4.Value());

  ParsedCookie pc5("=" + max_value + "; path=/foo;");
  EXPECT_TRUE(pc5.IsValid());
  EXPECT_EQ(max_value, pc5.Value());

  ParsedCookie pc6("=" + max_value + "X", &status);
  EXPECT_FALSE(pc6.IsValid());
  EXPECT_TRUE(status.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE));

  ParsedCookie pc7(almost_max_name + "=x");
  EXPECT_TRUE(pc7.IsValid());
  EXPECT_EQ(almost_max_name, pc7.Name());
  EXPECT_EQ("x", pc7.Value());

  ParsedCookie pc8(almost_max_name + "=x; path=/foo;");
  EXPECT_TRUE(pc8.IsValid());
  EXPECT_EQ(almost_max_name, pc8.Name());
  EXPECT_EQ("x", pc8.Value());

  ParsedCookie pc9(almost_max_name + "=xX", &status);
  EXPECT_FALSE(pc9.IsValid());
  EXPECT_TRUE(status.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE));

  ParsedCookie pc10("x=" + almost_max_value);
  EXPECT_TRUE(pc10.IsValid());
  EXPECT_EQ("x", pc10.Name());
  EXPECT_EQ(almost_max_value, pc10.Value());

  ParsedCookie pc11("x=" + almost_max_value + "; path=/foo;");
  EXPECT_TRUE(pc11.IsValid());
  EXPECT_EQ("x", pc11.Name());
  EXPECT_EQ(almost_max_value, pc11.Value());

  ParsedCookie pc12("xX=" + almost_max_value, &status);
  EXPECT_FALSE(pc12.IsValid());
  EXPECT_TRUE(status.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::
          EXCLUDE_NAME_VALUE_PAIR_EXCEEDS_MAX_SIZE));

  // Test attribute value size limits enforced by the constructor.
  std::string almost_max_path(ParsedCookie::kMaxCookieAttributeValueSize - 1,
                              'c');
  std::string max_path = "/" + almost_max_path;
  std::string too_long_path = "/X" + almost_max_path;

  ParsedCookie pc20("name=value; path=" + max_path);
  EXPECT_TRUE(pc20.IsValid());
  EXPECT_TRUE(pc20.HasPath());
  EXPECT_EQ("/" + almost_max_path, pc20.Path());

  ParsedCookie pc21("name=value; path=" + too_long_path, &status);
  EXPECT_TRUE(pc21.IsValid());
  EXPECT_FALSE(pc21.HasPath());
  EXPECT_TRUE(status.HasWarningReason(
      CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE));

  // NOTE: max_domain is based on the max attribute value as defined in
  // RFC6525bis, but this is larger than what is recommended by RFC1123.
  // In theory some browsers could restrict domains to that smaller size,
  // but ParsedCookie doesn't.
  std::string max_domain(ParsedCookie::kMaxCookieAttributeValueSize, 'd');
  max_domain.replace(ParsedCookie::kMaxCookieAttributeValueSize - 4, 4, ".com");
  std::string too_long_domain = "x" + max_domain;

  ParsedCookie pc30("name=value; domain=" + max_domain);
  EXPECT_TRUE(pc30.IsValid());
  EXPECT_TRUE(pc30.HasDomain());
  EXPECT_EQ(max_domain, pc30.Domain());

  ParsedCookie pc31("name=value; domain=" + too_long_domain);
  EXPECT_TRUE(pc31.IsValid());
  EXPECT_FALSE(pc31.HasDomain());
  EXPECT_TRUE(status.HasWarningReason(
      CookieInclusionStatus::WARN_ATTRIBUTE_VALUE_EXCEEDS_MAX_SIZE));

  std::string pc40_suffix = "; domain=example.com";

  ParsedCookie pc40("a=b" + pc40_suffix);
  EXPECT_TRUE(pc40.IsValid());

  // Test name + value size limits enforced by SetName / SetValue
  EXPECT_FALSE(pc40.SetName(max_name));
  EXPECT_EQ("a=b" + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  EXPECT_FALSE(pc40.SetValue(max_value));
  EXPECT_EQ("a=b" + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  EXPECT_TRUE(pc40.SetName(almost_max_name));
  EXPECT_EQ(almost_max_name + "=b" + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  EXPECT_FALSE(pc40.SetValue("xX"));
  EXPECT_EQ(almost_max_name + "=b" + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  EXPECT_TRUE(pc40.SetName("a"));
  EXPECT_TRUE(pc40.SetValue(almost_max_value));
  EXPECT_EQ("a=" + almost_max_value + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  EXPECT_FALSE(pc40.SetName("xX"));
  EXPECT_EQ("a=" + almost_max_value + pc40_suffix, pc40.ToCookieLine());
  EXPECT_TRUE(pc40.IsValid());

  std::string lots_of_spaces(ParsedCookie::kMaxCookieNamePlusValueSize, ' ');
  std::string test_str = "test";
  std::string padded_test_str = lots_of_spaces + test_str + lots_of_spaces;

  // Ensure that leading/trailing whitespace gets stripped before the length
  // calculations are enforced.
  ParsedCookie pc41("name=value");
  EXPECT_TRUE(pc41.SetName(padded_test_str));
  EXPECT_TRUE(pc41.SetValue(padded_test_str));
  EXPECT_EQ(test_str, pc41.Name());
  EXPECT_EQ(test_str, pc41.Value());

  std::string name_equals_value = "name=value";
  ParsedCookie pc50(name_equals_value);

  EXPECT_TRUE(pc50.SetPath(max_path));
  EXPECT_EQ(pc50.Path(), max_path);
  EXPECT_EQ(name_equals_value + "; path=" + max_path, pc50.ToCookieLine());
  EXPECT_TRUE(pc50.IsValid());

  // Test attribute value size limits enforced by SetPath
  EXPECT_FALSE(pc50.SetPath(too_long_path));
  EXPECT_EQ(pc50.Path(), max_path);
  EXPECT_EQ(name_equals_value + "; path=" + max_path, pc50.ToCookieLine());
  EXPECT_TRUE(pc50.IsValid());

  std::string test_path = "/test";
  std::string padded_test_path = lots_of_spaces + test_path + lots_of_spaces;

  EXPECT_TRUE(pc50.SetPath(padded_test_path));
  EXPECT_EQ(test_path, pc50.Path());

  ParsedCookie pc51(name_equals_value);

  EXPECT_TRUE(pc51.SetDomain(max_domain));
  EXPECT_EQ(pc51.Domain(), max_domain);
  EXPECT_EQ(name_equals_value + "; domain=" + max_domain, pc51.ToCookieLine());
  EXPECT_TRUE(pc51.IsValid());

  // Test attribute value size limits enforced by SetDomain
  EXPECT_FALSE(pc51.SetDomain(too_long_domain));
  EXPECT_EQ(pc51.Domain(), max_domain);
  EXPECT_EQ(name_equals_value + "; domain=" + max_domain, pc51.ToCookieLine());
  EXPECT_TRUE(pc51.IsValid());

  std::string test_domain = "example.com";
  std::string padded_test_domain =
      lots_of_spaces + test_domain + lots_of_spaces;

  EXPECT_TRUE(pc51.SetDomain(padded_test_domain));
  EXPECT_EQ(test_domain, pc51.Domain());
}

TEST(ParsedCookieTest, EmbeddedTerminator) {
  using std::string_literals::operator""s;

  CookieInclusionStatus status1;
  CookieInclusionStatus status2;
  CookieInclusionStatus status3;
  ParsedCookie pc1("AAA=BB\0ZYX"s, &status1);
  ParsedCookie pc2("AAA=BB\rZYX"s, &status2);
  ParsedCookie pc3("AAA=BB\nZYX"s, &status3);

  EXPECT_FALSE(pc1.IsValid());
  EXPECT_FALSE(pc2.IsValid());
  EXPECT_FALSE(pc3.IsValid());
  EXPECT_TRUE(status1.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));
  EXPECT_TRUE(status2.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));
  EXPECT_TRUE(status3.HasOnlyExclusionReason(
      CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));
}

TEST(ParsedCookieTest, ParseTokensAndValues) {
  EXPECT_EQ("hello", ParsedCookie::ParseTokenString("hello\nworld"));
  EXPECT_EQ("fs!!@", ParsedCookie::ParseTokenString("fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            ParsedCookie::ParseTokenString("hello world\tgood\rbye"));
  EXPECT_EQ("A", ParsedCookie::ParseTokenString("A=B=C;D=E"));
  EXPECT_EQ("hello", ParsedCookie::ParseValueString("hello\nworld"));
  EXPECT_EQ("fs!!@", ParsedCookie::ParseValueString("fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            ParsedCookie::ParseValueString("hello world\tgood\rbye"));
  EXPECT_EQ("A=B=C", ParsedCookie::ParseValueString("A=B=C;D=E"));
}

TEST(ParsedCookieTest, SerializeCookieLine) {
  const char input[] =
      "ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
      "path=/  ;  priority=low  ;  ";
  const char output[] =
      "ANCUUID=zohNumRKgI0oxyhSsV3Z7D; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT; "
      "path=/; priority=low";
  ParsedCookie pc(input);
  EXPECT_EQ(output, pc.ToCookieLine());
}

TEST(ParsedCookieTest, SetNameAndValue) {
  ParsedCookie cookie("a=b");
  EXPECT_TRUE(cookie.IsValid());
  EXPECT_TRUE(cookie.SetDomain("foobar.com"));
  EXPECT_TRUE(cookie.SetName("name"));
  EXPECT_TRUE(cookie.SetValue("value"));
  EXPECT_EQ("name=value; domain=foobar.com", cookie.ToCookieLine());
  EXPECT_TRUE(cookie.IsValid());

  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  // Set invalid name / value.
  EXPECT_FALSE(pc.SetName("foo\nbar"));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetName("foo\rbar"));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue(std::string("foo\0bar", 7)));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set previously invalid name / value.
  EXPECT_TRUE(pc.SetName("@foobar"));
  EXPECT_EQ("@foobar=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetName("foo bar"));
  EXPECT_EQ("foo bar=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetName("\"foobar"));
  EXPECT_EQ("\"foobar=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue("foo bar"));
  EXPECT_EQ("\"foobar=foo bar", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue("\"foobar"));
  EXPECT_EQ("\"foobar=\"foobar", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetName("  foo bar  "));
  EXPECT_EQ("foo bar=\"foobar", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue("  foo bar  "));
  EXPECT_EQ("foo bar=foo bar", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set valid name / value.
  EXPECT_TRUE(pc.SetValue("value"));
  EXPECT_TRUE(pc.SetName(std::string()));
  EXPECT_EQ("=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetName("test"));
  EXPECT_EQ("test=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue("\"foobar\""));
  EXPECT_EQ("test=\"foobar\"", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue(std::string()));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Ensure that failure occurs when trying to set a name containing '='.
  EXPECT_FALSE(pc.SetName("invalid=name"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Ensure that trying to set a name containing ';' fails.
  EXPECT_FALSE(pc.SetName("invalid;name"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue("invalid;value"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Ensure tab characters are treated as control characters.
  // TODO(crbug.com/40191620) Update this such that tab characters are allowed
  // and are handled correctly.
  EXPECT_FALSE(pc.SetName("\tinvalid\t"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue("\tinvalid\t"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetName("na\tme"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue("val\tue"));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());
}

TEST(ParsedCookieTest, SetAttributes) {
  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  // Clear an unset attribute.
  EXPECT_TRUE(pc.SetDomain(std::string()));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set a string containing an invalid character
  EXPECT_FALSE(pc.SetDomain("foo;bar"));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set all other attributes and check that they are appended in order.
  EXPECT_TRUE(pc.SetDomain("domain.com"));
  EXPECT_TRUE(pc.SetPath("/"));
  EXPECT_TRUE(pc.SetExpires("Sun, 18-Apr-2027 21:06:29 GMT"));
  EXPECT_TRUE(pc.SetMaxAge("12345"));
  EXPECT_TRUE(pc.SetIsSecure(true));
  EXPECT_TRUE(pc.SetIsHttpOnly(true));
  EXPECT_TRUE(pc.SetIsHttpOnly(true));
  EXPECT_TRUE(pc.SetSameSite("LAX"));
  EXPECT_TRUE(pc.SetPriority("HIGH"));
  EXPECT_TRUE(pc.SetIsPartitioned(true));
  EXPECT_EQ(
      "name=value; domain=domain.com; path=/; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
      "httponly; samesite=LAX; priority=HIGH; partitioned",
      pc.ToCookieLine());
  EXPECT_TRUE(pc.HasDomain());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasMaxAge());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite());
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, pc.Priority());

  // Modify one attribute in the middle.
  EXPECT_TRUE(pc.SetPath("/foo"));
  EXPECT_TRUE(pc.HasDomain());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/foo", pc.Path());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(
      "name=value; domain=domain.com; path=/foo; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
      "httponly; samesite=LAX; priority=HIGH; partitioned",
      pc.ToCookieLine());

  // Set priority to medium.
  EXPECT_TRUE(pc.SetPriority("medium"));
  EXPECT_EQ(CookiePriority::COOKIE_PRIORITY_MEDIUM, pc.Priority());
  EXPECT_EQ(
      "name=value; domain=domain.com; path=/foo; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
      "httponly; samesite=LAX; priority=medium; partitioned",
      pc.ToCookieLine());

  // Clear attribute from the end.
  EXPECT_TRUE(pc.SetIsPartitioned(false));
  EXPECT_FALSE(pc.IsPartitioned());
  EXPECT_EQ(
      "name=value; domain=domain.com; path=/foo; "
      "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
      "httponly; samesite=LAX; priority=medium",
      pc.ToCookieLine());

  // Clear the rest and change the name and value.
  EXPECT_TRUE(pc.SetDomain(std::string()));
  EXPECT_TRUE(pc.SetPath(std::string()));
  EXPECT_TRUE(pc.SetExpires(std::string()));
  EXPECT_TRUE(pc.SetMaxAge(std::string()));
  EXPECT_TRUE(pc.SetIsSecure(false));
  EXPECT_TRUE(pc.SetIsHttpOnly(false));
  EXPECT_TRUE(pc.SetSameSite(std::string()));
  EXPECT_TRUE(pc.SetName("name2"));
  EXPECT_TRUE(pc.SetValue("value2"));
  EXPECT_TRUE(pc.SetPriority(std::string()));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasExpires());
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_FALSE(pc.IsHttpOnly());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite());
  EXPECT_TRUE(pc.SetIsPartitioned(false));
  EXPECT_EQ("name2=value2", pc.ToCookieLine());
  EXPECT_FALSE(pc.IsPartitioned());
}

// Setting the domain attribute to the empty string should be valid.
TEST(ParsedCookieTest, EmptyDomainAttributeValid) {
  ParsedCookie pc("name=value; domain=");
  EXPECT_TRUE(pc.IsValid());
}

// Set the domain attribute twice in a cookie line. If the second attribute's
// value is empty, it should equal the empty string.
TEST(ParsedCookieTest, MultipleDomainAttributes) {
  ParsedCookie pc1("name=value; domain=foo.com; domain=bar.com");
  EXPECT_EQ("bar.com", pc1.Domain());
  ParsedCookie pc2("name=value; domain=foo.com; domain=");
  EXPECT_EQ(std::string(), pc2.Domain());
}

TEST(ParsedCookieTest, SetPriority) {
  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());

  // Test each priority, expect case-insensitive compare.
  EXPECT_TRUE(pc.SetPriority("high"));
  EXPECT_EQ("name=value; priority=high", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_HIGH, pc.Priority());

  EXPECT_TRUE(pc.SetPriority("mEDium"));
  EXPECT_EQ("name=value; priority=mEDium", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_MEDIUM, pc.Priority());

  EXPECT_TRUE(pc.SetPriority("LOW"));
  EXPECT_EQ("name=value; priority=LOW", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_LOW, pc.Priority());

  // Interpret invalid priority values as COOKIE_PRIORITY_DEFAULT.
  EXPECT_TRUE(pc.SetPriority("Blah"));
  EXPECT_EQ("name=value; priority=Blah", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());

  EXPECT_TRUE(pc.SetPriority("lowerest"));
  EXPECT_EQ("name=value; priority=lowerest", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());

  EXPECT_TRUE(pc.SetPriority(""));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_EQ(COOKIE_PRIORITY_DEFAULT, pc.Priority());
}

TEST(ParsedCookieTest, SetSameSite) {
  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite());

  // Test each samesite directive, expect case-insensitive compare.
  EXPECT_TRUE(pc.SetSameSite("strict"));
  EXPECT_EQ("name=value; samesite=strict", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::STRICT_MODE, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetSameSite("lAx"));
  EXPECT_EQ("name=value; samesite=lAx", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetSameSite("LAX"));
  EXPECT_EQ("name=value; samesite=LAX", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetSameSite("None"));
  EXPECT_EQ("name=value; samesite=None", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetSameSite("NONE"));
  EXPECT_EQ("name=value; samesite=NONE", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  // Remove the SameSite attribute.
  EXPECT_TRUE(pc.SetSameSite(""));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetSameSite("Blah"));
  EXPECT_EQ("name=value; samesite=Blah", pc.ToCookieLine());
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite());
  EXPECT_TRUE(pc.IsValid());
}

// Test that the correct enum value is returned for the SameSite attribute
// string.
TEST(ParsedCookieTest, CookieSameSiteStringEnum) {
  ParsedCookie pc("name=value; SameSite");
  CookieSameSiteString actual = CookieSameSiteString::kLax;
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kEmptyString, actual);

  pc.SetSameSite("Strict");
  EXPECT_EQ(CookieSameSite::STRICT_MODE, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kStrict, actual);

  pc.SetSameSite("Lax");
  EXPECT_EQ(CookieSameSite::LAX_MODE, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kLax, actual);

  pc.SetSameSite("None");
  EXPECT_EQ(CookieSameSite::NO_RESTRICTION, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kNone, actual);

  pc.SetSameSite("Extended");
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kExtended, actual);

  pc.SetSameSite("Bananas");
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kUnrecognized, actual);

  ParsedCookie pc2("no_samesite=1");
  EXPECT_EQ(CookieSameSite::UNSPECIFIED, pc2.SameSite(&actual));
  EXPECT_EQ(CookieSameSiteString::kUnspecified, actual);
}

TEST(ParsedCookieTest, SettersInputValidation) {
  ParsedCookie pc("name=foobar");
  EXPECT_TRUE(pc.SetPath("baz"));
  EXPECT_EQ(pc.ToCookieLine(), "name=foobar; path=baz");

  EXPECT_TRUE(pc.SetPath("  baz "));
  EXPECT_EQ(pc.ToCookieLine(), "name=foobar; path=baz");

  EXPECT_TRUE(pc.SetPath("     "));
  EXPECT_EQ(pc.ToCookieLine(), "name=foobar");

  EXPECT_TRUE(pc.SetDomain("  baz "));
  EXPECT_EQ(pc.ToCookieLine(), "name=foobar; domain=baz");

  // Invalid characters
  EXPECT_FALSE(pc.SetPath("  baz\n "));
  EXPECT_FALSE(pc.SetPath("f;oo"));
  EXPECT_FALSE(pc.SetPath("\r"));
  EXPECT_FALSE(pc.SetPath("\a"));
  EXPECT_FALSE(pc.SetPath("\t"));
  EXPECT_FALSE(pc.SetSameSite("\r"));
}

TEST(ParsedCookieTest, ToCookieLineSpecialTokens) {
  // Special tokens "secure", "httponly" should be treated as
  // any other name when they are in the first position.
  {
    ParsedCookie pc("");
    pc.SetName("secure");
    EXPECT_EQ(pc.ToCookieLine(), "secure=");
  }
  {
    ParsedCookie pc("secure");
    EXPECT_EQ(pc.ToCookieLine(), "=secure");
  }
  {
    ParsedCookie pc("secure=foo");
    EXPECT_EQ(pc.ToCookieLine(), "secure=foo");
  }
  {
    ParsedCookie pc("foo=secure");
    EXPECT_EQ(pc.ToCookieLine(), "foo=secure");
  }
  {
    ParsedCookie pc("httponly=foo");
    EXPECT_EQ(pc.ToCookieLine(), "httponly=foo");
  }
  {
    ParsedCookie pc("foo");
    pc.SetName("secure");
    EXPECT_EQ(pc.ToCookieLine(), "secure=foo");
  }
  {
    ParsedCookie pc("bar");
    pc.SetName("httponly");
    EXPECT_EQ(pc.ToCookieLine(), "httponly=bar");
  }
  {
    ParsedCookie pc("foo=bar; baz=bob");
    EXPECT_EQ(pc.ToCookieLine(), "foo=bar; baz=bob");
  }
  // Outside of the first position, the value associated with a special name
  // should not be printed.
  {
    ParsedCookie pc("name=foo; secure");
    EXPECT_EQ(pc.ToCookieLine(), "name=foo; secure");
  }
  {
    ParsedCookie pc("name=foo; secure=bar");
    EXPECT_EQ(pc.ToCookieLine(), "name=foo; secure");
  }
  {
    ParsedCookie pc("name=foo; httponly=baz");
    EXPECT_EQ(pc.ToCookieLine(), "name=foo; httponly");
  }
  {
    ParsedCookie pc("name=foo; bar=secure");
    EXPECT_EQ(pc.ToCookieLine(), "name=foo; bar=secure");
  }
  // Repeated instances of the special tokens are also fine.
  {
    ParsedCookie pc("name=foo; secure; secure=yesplease; secure; secure");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_TRUE(pc.IsSecure());
    EXPECT_FALSE(pc.IsHttpOnly());
  }
  {
    ParsedCookie pc("partitioned=foo");
    EXPECT_EQ("partitioned", pc.Name());
    EXPECT_EQ("foo", pc.Value());
    EXPECT_FALSE(pc.IsPartitioned());
  }
  {
    ParsedCookie pc("partitioned=");
    EXPECT_EQ("partitioned", pc.Name());
    EXPECT_EQ("", pc.Value());
    EXPECT_FALSE(pc.IsPartitioned());
  }
  {
    ParsedCookie pc("=partitioned");
    EXPECT_EQ("", pc.Name());
    EXPECT_EQ("partitioned", pc.Value());
    EXPECT_FALSE(pc.IsPartitioned());
  }
  {
    ParsedCookie pc(
        "partitioned; partitioned; secure; httponly; httponly; secure");
    EXPECT_EQ("", pc.Name());
    EXPECT_EQ("partitioned", pc.Value());
    EXPECT_TRUE(pc.IsPartitioned());
  }
}

TEST(ParsedCookieTest, SameSiteValues) {
  struct TestCase {
    const char* cookie;
    bool valid;
    CookieSameSite mode;
  } cases[]{{"n=v; samesite=strict", true, CookieSameSite::STRICT_MODE},
            {"n=v; samesite=lax", true, CookieSameSite::LAX_MODE},
            {"n=v; samesite=none", true, CookieSameSite::NO_RESTRICTION},
            {"n=v; samesite=boo", true, CookieSameSite::UNSPECIFIED},
            {"n=v; samesite", true, CookieSameSite::UNSPECIFIED},
            {"n=v", true, CookieSameSite::UNSPECIFIED}};

  for (const auto& test : cases) {
    SCOPED_TRACE(test.cookie);
    ParsedCookie pc(test.cookie);
    EXPECT_EQ(test.valid, pc.IsValid());
    EXPECT_EQ(test.mode, pc.SameSite());
  }
}

TEST(ParsedCookieTest, InvalidNonAlphanumericChars) {
  // clang-format off
  const char* cases[] = {
      "name=\x05",
      "name=foo\x1c" "bar",
      "name=foobar\x11",
      "name=\x02" "foobar",
      "\x05=value",
      "foo\x05" "bar=value",
      "foobar\x05" "=value",
      "\x05" "foobar=value",
      "foo\x05" "bar=foo\x05" "bar",
      "foo=ba,ba\x05" "z=boo",
      "foo=ba,baz=bo\x05" "o",
      "foo=ba,ba\05" "z=bo\x05" "o",
      "foo=ba,ba\x7F" "z=bo",
      "fo\x7F" "o=ba,z=bo",
      "foo=bar\x7F" ";z=bo",
  };
  // clang-format on

  for (size_t i = 0; i < std::size(cases); i++) {
    SCOPED_TRACE(testing::Message()
                 << "Test case #" << base::NumberToString(i + 1));
    CookieInclusionStatus status;
    ParsedCookie pc(cases[i], &status);
    EXPECT_FALSE(pc.IsValid());
    EXPECT_TRUE(status.HasOnlyExclusionReason(
        CookieInclusionStatus::ExclusionReason::EXCLUDE_DISALLOWED_CHARACTER));
  }
}

TEST(ParsedCookieTest, ValidNonAlphanumericChars) {
  // Note that some of these words are pasted backwords thanks to poor vim
  // bidi support. This should not affect the tests, however.
  const char pc1_literal[] = "name=العربية";
  const char pc2_literal[] = "name=普通話";
  const char pc3_literal[] = "name=ภาษาไทย";
  const char pc4_literal[] = "name=עִבְרִית";
  const char pc5_literal[] = "العربية=value";
  const char pc6_literal[] = "普通話=value";
  const char pc7_literal[] = "ภาษาไทย=value";
  const char pc8_literal[] = "עִבְרִית=value";
  const char pc9_literal[] = "@foo=bar";

  ParsedCookie pc1(pc1_literal);
  ParsedCookie pc2(pc2_literal);
  ParsedCookie pc3(pc3_literal);
  ParsedCookie pc4(pc4_literal);
  ParsedCookie pc5(pc5_literal);
  ParsedCookie pc6(pc6_literal);
  ParsedCookie pc7(pc7_literal);
  ParsedCookie pc8(pc8_literal);
  ParsedCookie pc9(pc9_literal);

  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ(pc1_literal, pc1.ToCookieLine());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ(pc2_literal, pc2.ToCookieLine());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_EQ(pc3_literal, pc3.ToCookieLine());
  EXPECT_TRUE(pc4.IsValid());
  EXPECT_EQ(pc4_literal, pc4.ToCookieLine());
  EXPECT_TRUE(pc5.IsValid());
  EXPECT_EQ(pc5_literal, pc5.ToCookieLine());
  EXPECT_TRUE(pc6.IsValid());
  EXPECT_EQ(pc6_literal, pc6.ToCookieLine());
  EXPECT_TRUE(pc7.IsValid());
  EXPECT_EQ(pc7_literal, pc7.ToCookieLine());
  EXPECT_TRUE(pc8.IsValid());
  EXPECT_EQ(pc8_literal, pc8.ToCookieLine());
  EXPECT_TRUE(pc9.IsValid());
  EXPECT_EQ(pc9_literal, pc9.ToCookieLine());

  EXPECT_TRUE(pc1.SetValue(pc1.Value()));
  EXPECT_EQ(pc1_literal, pc1.ToCookieLine());
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_TRUE(pc2.SetValue(pc2.Value()));
  EXPECT_EQ(pc2_literal, pc2.ToCookieLine());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_TRUE(pc3.SetValue(pc3.Value()));
  EXPECT_EQ(pc3_literal, pc3.ToCookieLine());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_TRUE(pc4.SetValue(pc4.Value()));
  EXPECT_EQ(pc4_literal, pc4.ToCookieLine());
  EXPECT_TRUE(pc4.IsValid());
  EXPECT_TRUE(pc5.SetName(pc5.Name()));
  EXPECT_EQ(pc5_literal, pc5.ToCookieLine());
  EXPECT_TRUE(pc5.IsValid());
  EXPECT_TRUE(pc6.SetName(pc6.Name()));
  EXPECT_EQ(pc6_literal, pc6.ToCookieLine());
  EXPECT_TRUE(pc6.IsValid());
  EXPECT_TRUE(pc7.SetName(pc7.Name()));
  EXPECT_EQ(pc7_literal, pc7.ToCookieLine());
  EXPECT_TRUE(pc7.IsValid());
  EXPECT_TRUE(pc8.SetName(pc8.Name()));
  EXPECT_EQ(pc8_literal, pc8.ToCookieLine());
  EXPECT_TRUE(pc8.IsValid());
  EXPECT_TRUE(pc9.SetName(pc9.Name()));
  EXPECT_EQ(pc9_literal, pc9.ToCookieLine());
  EXPECT_TRUE(pc9.IsValid());
}

TEST(ParsedCookieTest, PreviouslyTruncatingCharInCookieLine) {
  // Test scenarios where a control char may appear at start, middle and end of
  // a cookie line. Control char array with NULL (\x0), CR (\xD), LF (xA),
  // HT (\x9) and BS (\x1B).
  const struct {
    const char ctlChar;
    bool invalid_character;
  } kTests[] = {{'\x0', true},
                {'\xD', true},
                {'\xA', true},
                {'\x9', false},
                {'\x1B', false}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message() << "Using test.ctlChar == "
                                    << base::NumberToString(test.ctlChar));
    std::string ctl_string(1, test.ctlChar);
    std::string ctl_at_start_cookie_string =
        base::StrCat({ctl_string, "foo=bar"});
    ParsedCookie ctl_at_start_cookie(ctl_at_start_cookie_string);
    // Lots of factors determine whether IsValid() is true here:
    //
    //  - For the tab character ('\x9), leading whitespace is valid and the
    //  spec indicates that it should just be removed and the cookie parsed
    //  normally. Thus, in this case the cookie is always valid.
    //
    //  - For control characters that historically truncated the cookie, they
    //  now cause the cookie to be deemed invalid.
    //
    //  - For other control characters the cookie is always treated as invalid.
    EXPECT_EQ(ctl_at_start_cookie.IsValid(), test.ctlChar == '\x9');

    std::string ctl_at_middle_cookie_string =
        base::StrCat({"foo=bar;", ctl_string, "secure"});
    ParsedCookie ctl_at_middle_cookie(ctl_at_middle_cookie_string);
    if (test.invalid_character) {
      EXPECT_EQ(ctl_at_middle_cookie.IsValid(), false);
    }

    std::string ctl_at_end_cookie_string =
        base::StrCat({"foo=bar;", "secure;", ctl_string});
    ParsedCookie ctl_at_end_cookie(ctl_at_end_cookie_string);
    if (test.invalid_character) {
      EXPECT_EQ(ctl_at_end_cookie.IsValid(), false);
    }
  }

  // Test if there are multiple control characters that terminate.
  std::string ctls_cookie_string = "foo=bar;\xA\xD";
  ParsedCookie ctls_cookie(ctls_cookie_string);
  EXPECT_EQ(ctls_cookie.IsValid(), false);
}

TEST(ParsedCookieTest, HtabInNameOrValue) {
  std::string no_htab_string = "foo=bar";
  ParsedCookie no_htab(no_htab_string);
  EXPECT_FALSE(no_htab.HasInternalHtab());

  std::string htab_leading_trailing_string = "\tfoo=bar\t";
  ParsedCookie htab_leading_trailing(htab_leading_trailing_string);
  EXPECT_FALSE(htab_leading_trailing.HasInternalHtab());

  std::string htab_name_string = "f\too=bar";
  ParsedCookie htab_name(htab_name_string);
  EXPECT_TRUE(htab_name.HasInternalHtab());

  std::string htab_value_string = "foo=b\tar";
  ParsedCookie htab_value(htab_value_string);
  EXPECT_TRUE(htab_value.HasInternalHtab());
}

}  // namespace net

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/url_util.h"

#include <optional>
#include <ostream>

#include "base/format_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_features.h"
#include "url/url_util.h"

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {
namespace {

TEST(UrlUtilTest, AppendQueryParameter) {
  // Appending a name-value pair to a URL without a query component.
  EXPECT_EQ("http://example.com/path?name=value",
            AppendQueryParameter(GURL("http://example.com/path"),
                                 "name", "value").spec());

  // Appending a name-value pair to a URL with a query component.
  // The original component should be preserved, and the new pair should be
  // appended with '&'.
  EXPECT_EQ("http://example.com/path?existing=one&name=value",
            AppendQueryParameter(GURL("http://example.com/path?existing=one"),
                                 "name", "value").spec());

  // Appending a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?existing=one&na+me=v.alue%3D",
            AppendQueryParameter(GURL("http://example.com/path?existing=one"),
                                 "na me", "v.alue=")
                .spec());
}

TEST(UrlUtilTest, AppendOrReplaceQueryParameter) {
  // Appending a name-value pair to a URL without a query component.
  EXPECT_EQ("http://example.com/path?name=value",
            AppendOrReplaceQueryParameter(GURL("http://example.com/path"),
                                 "name", "value").spec());

  // Appending a name-value pair to a URL with a query component.
  // The original component should be preserved, and the new pair should be
  // appended with '&'.
  EXPECT_EQ("http://example.com/path?existing=one&name=value",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one"),
          "name", "value").spec());

  // Appending a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?existing=one&na+me=v.alue%3D",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one"),
          "na me", "v.alue=").spec());

  // Replace value of an existing paramater.
  EXPECT_EQ("http://example.com/path?existing=one&name=new",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one&name=old"),
          "name", "new").spec());

  // Replace a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?na+me=n.ew%3D&existing=one",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?na+me=old&existing=one"),
          "na me", "n.ew=").spec());

  // Replace the value of first parameter with this name only.
  EXPECT_EQ("http://example.com/path?name=new&existing=one&name=old",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?name=old&existing=one&name=old"),
          "name", "new").spec());

  // Preserve the content of the original params regardless of our failure to
  // interpret them correctly.
  EXPECT_EQ("http://example.com/path?bar&name=new&left=&"
            "=right&=&&name=again",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?bar&name=old&left=&"
                "=right&=&&name=again"),
          "name", "new").spec());

  // ----- Removing the key using nullopt value -----

  // Removes the name-value pair from the URL preserving other query parameters.
  EXPECT_EQ("http://example.com/path?abc=xyz",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/path?name=value&abc=xyz"), "name",
                std::nullopt)
                .spec());

  // Removes the name-value pair from the URL.
  EXPECT_EQ("http://example.com/path?",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/path?existing=one"), "existing",
                std::nullopt)
                .spec());

  // Removes the first name-value pair.
  EXPECT_EQ("http://example.com/path?c=d&e=f",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/path?a=b&c=d&e=f"), "a", std::nullopt)
                .spec());

  // Removes a name-value pair in between two query params.
  EXPECT_EQ(
      "http://example.com/path?existing=one&hello=world",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?existing=one&replace=sure&hello=world"),
          "replace", std::nullopt)
          .spec());

  // Removes the last name-value pair.
  EXPECT_EQ("http://example.com/path?existing=one",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/path?existing=one&replace=sure"),
                "replace", std::nullopt)
                .spec());

  // Removing a name-value pair with unsafe characters included. The
  // unsafe characters should be escaped.
  EXPECT_EQ("http://example.com/path?existing=one&hello=world",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/"
                     "path?existing=one&na+me=v.alue%3D&hello=world"),
                "na me", std::nullopt)
                .spec());

  // Does nothing if the provided query param key does not exist.
  EXPECT_EQ("http://example.com/path?existing=one&name=old",
            AppendOrReplaceQueryParameter(
                GURL("http://example.com/path?existing=one&name=old"), "old",
                std::nullopt)
                .spec());

  // Remove the value of first parameter with this name only.
  EXPECT_EQ(
      "http://example.com/path?existing=one&name=old",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?name=something&existing=one&name=old"),
          "name", std::nullopt)
          .spec());

  // Preserve the content of the original params regardless of our failure to
  // interpret them correctly.
  EXPECT_EQ(
      "http://example.com/path?bar&left=&"
      "=right&=&&name=again",
      AppendOrReplaceQueryParameter(
          GURL("http://example.com/path?bar&name=old&left=&"
               "=right&=&&name=again"),
          "name", std::nullopt)
          .spec());
}

TEST(UrlUtilTest, AppendOrReplaceRef) {
  // Setting a new ref should append it.
  EXPECT_EQ("http://example.com/path#ref",
            AppendOrReplaceRef(GURL("http://example.com/path"), "ref").spec());

  // Setting a ref over an existing one should replace it.
  EXPECT_EQ("http://example.com/path#ref",
            AppendOrReplaceRef(GURL("http://example.com/path#old_ref"), "ref")
                .spec());

  // Setting a ref on a url with existing query parameters should simply append
  // it at the end
  EXPECT_EQ(
      "http://example.com/path?query=value#ref",
      AppendOrReplaceRef(GURL("http://example.com/path?query=value#ref"), "ref")
          .spec());

  // Setting a ref on a url with existing query parameters and with special
  // encoded characters: `special-chars?query=value#ref chars%\";'`
  EXPECT_EQ(
      "http://example.com/special-chars?query=value#ref%20chars%%22;'",
      AppendOrReplaceRef(GURL("http://example.com/special-chars?query=value"),
                         "ref chars%\";'")
          .spec());

  // Testing adding a ref to a URL with specially encoded characters.
  // `special chars%\";'?query=value#ref`
  EXPECT_EQ(
      "http://example.com/special%20chars%%22;'?query=value#ref",
      AppendOrReplaceRef(
          GURL("http://example.com/special chars%\";'?query=value"), "ref")
          .spec());
}

TEST(UrlUtilTest, GetValueForKeyInQuery) {
  GURL url("http://example.com/path?name=value&boolParam&"
           "url=http://test.com/q?n1%3Dv1%26n2");
  std::string value;

  // False when getting a non-existent query param.
  EXPECT_FALSE(GetValueForKeyInQuery(url, "non-exist", &value));

  // True when query param exist.
  EXPECT_TRUE(GetValueForKeyInQuery(url, "name", &value));
  EXPECT_EQ("value", value);

  EXPECT_TRUE(GetValueForKeyInQuery(url, "boolParam", &value));
  EXPECT_EQ("", value);

  EXPECT_TRUE(GetValueForKeyInQuery(url, "url", &value));
  EXPECT_EQ("http://test.com/q?n1=v1&n2", value);
}

TEST(UrlUtilTest, GetValueForKeyInQueryInvalidURL) {
  GURL url("http://%01/?test");
  std::string value;

  // Always false when parsing an invalid URL.
  EXPECT_FALSE(GetValueForKeyInQuery(url, "test", &value));
}

TEST(UrlUtilTest, ParseQuery) {
  const GURL url("http://example.com/path?name=value&boolParam&"
                 "url=http://test.com/q?n1%3Dv1%26n2&"
                 "multikey=value1&multikey=value2&multikey");
  QueryIterator it(url);

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("name", it.GetKey());
  EXPECT_EQ("value", it.GetValue());
  EXPECT_EQ("value", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("boolParam", it.GetKey());
  EXPECT_EQ("", it.GetValue());
  EXPECT_EQ("", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("url", it.GetKey());
  EXPECT_EQ("http://test.com/q?n1%3Dv1%26n2", it.GetValue());
  EXPECT_EQ("http://test.com/q?n1=v1&n2", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("value1", it.GetValue());
  EXPECT_EQ("value1", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("value2", it.GetValue());
  EXPECT_EQ("value2", it.GetUnescapedValue());
  it.Advance();

  ASSERT_FALSE(it.IsAtEnd());
  EXPECT_EQ("multikey", it.GetKey());
  EXPECT_EQ("", it.GetValue());
  EXPECT_EQ("", it.GetUnescapedValue());
  it.Advance();

  EXPECT_TRUE(it.IsAtEnd());
}

TEST(UrlUtilTest, ParseQueryInvalidURL) {
  const GURL url("http://%01/?test");
  QueryIterator it(url);
  EXPECT_TRUE(it.IsAtEnd());
}

TEST(UrlUtilTest, ParseHostAndPort) {
  const struct {
    const char* const input;
    bool success;
    const char* const expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "1080:0:0:0:8:800:200C:4171",
      11
    },
    {
      "[1080:0:0:0:8:800:200C:4171]",
      true,
      "1080:0:0:0:8:800:200C:4171",
      -1
    },

    // Because no validation is done on the host, the following are accepted,
    // even though they are invalid names.
    {"]", true, "]", -1},
    {"::1", true, ":", 1},
    // Invalid inputs:
    {"foo:bar", false, "", -1},
    {"foo:", false, "", -1},
    {":", false, "", -1},
    {":80", false, "", -1},
    {"", false, "", -1},
    {"porttoolong:300000", false, "", -1},
    {"usrname@host", false, "", -1},
    {"usrname:password@host", false, "", -1},
    {":password@host", false, "", -1},
    {":password@host:80", false, "", -1},
    {":password@host", false, "", -1},
    {"@host", false, "", -1},
    {"[", false, "", -1},
    {"[]", false, "", -1},
  };

  for (const auto& test : tests) {
    std::string host;
    int port;
    bool ok = ParseHostAndPort(test.input, &host, &port);
    EXPECT_EQ(test.success, ok);

    if (test.success) {
      EXPECT_EQ(test.expected_host, host);
      EXPECT_EQ(test.expected_port, port);
    }
  }
}
TEST(UrlUtilTest, GetHostAndPort) {
  const struct {
    GURL url;
    const char* const expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (const auto& test : tests) {
    std::string host_and_port = GetHostAndPort(test.url);
    EXPECT_EQ(std::string(test.expected_host_and_port), host_and_port);
  }
}

TEST(UrlUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* const expected_host_and_port;
  } tests[] = {
      {GURL("http://www.foo.com/x"), "www.foo.com"},
      {GURL("http://www.foo.com:21/x"), "www.foo.com:21"},
      {GURL("http://www.foo.com:443/x"), "www.foo.com:443"},

      {GURL("https://www.foo.com/x"), "www.foo.com"},
      {GURL("https://www.foo.com:80/x"), "www.foo.com:80"},

      // For IPv6 literals should always include the brackets.
      {GURL("http://[1::2]/x"), "[1::2]"},
      {GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (const auto& test : tests) {
    EXPECT_EQ(test.expected_host_and_port, GetHostAndOptionalPort(test.url));
    // Also test the SchemeHostPort variant.
    EXPECT_EQ(test.expected_host_and_port,
              GetHostAndOptionalPort(url::SchemeHostPort(test.url)));
  }
}

TEST(UrlUtilTest, GetHostOrSpecFromURL) {
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com/test")));
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com./test")));
  EXPECT_EQ("file:///tmp/test.html",
            GetHostOrSpecFromURL(GURL("file:///tmp/test.html")));
}

TEST(UrlUtilTest, GetSuperdomain) {
  struct {
    const char* const domain;
    const char* const expected_superdomain;
  } tests[] = {
      // Basic cases
      {"foo.bar.example", "bar.example"},
      {"bar.example", "example"},
      {"example", ""},

      // Returned value may be an eTLD.
      {"google.com", "com"},
      {"google.co.uk", "co.uk"},

      // Weird cases.
      {"", ""},
      {"has.trailing.dot.", "trailing.dot."},
      {"dot.", ""},
      {".has.leading.dot", "has.leading.dot"},
      {".", ""},
      {"..", "."},
      {"127.0.0.1", "0.0.1"},
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.expected_superdomain, GetSuperdomain(test.domain));
  }
}

TEST(UrlUtilTest, IsSubdomainOf) {
  struct {
    const char* subdomain;
    const char* superdomain;
    bool is_subdomain;
  } tests[] = {
      {"bar.foo.com", "foo.com", true},
      {"barfoo.com", "foo.com", false},
      {"bar.foo.com", "com", true},
      {"bar.foo.com", "other.com", false},
      {"bar.foo.com", "bar.foo.com", true},
      {"bar.foo.com", "baz.foo.com", false},
      {"bar.foo.com", "baz.bar.foo.com", false},
      {"bar.foo.com", "ar.foo.com", false},
      {"foo.com", "foo.com.", false},
      {"bar.foo.com", "foo.com.", false},
      {"", "", true},
      {"a", "", false},
      {"", "a", false},
      {"127.0.0.1", "0.0.1", true},  // Don't do this...
  };

  for (const auto& test : tests) {
    EXPECT_EQ(test.is_subdomain,
              IsSubdomainOf(test.subdomain, test.superdomain));
  }
}

TEST(UrlUtilTest, CompliantHost) {
  struct {
    const char* const host;
    bool expected_output;
  } compliant_host_cases[] = {
      {"", false},
      {"a", true},
      {"-", false},
      {"_", false},
      {".", false},
      {"9", true},
      {"9a", true},
      {"9_", true},
      {"a.", true},
      {".a", false},
      {"a.a", true},
      {"9.a", true},
      {"a.9", true},
      {"_9a", false},
      {"-9a", false},
      {"a.a9", true},
      {"_.9a", true},
      {"a.-a9", false},
      {"a+9a", false},
      {"-a.a9", true},
      {"a_.a9", true},
      {"1-.a-b", true},
      {"1_.a-b", true},
      {"1-2.a_b", true},
      {"a.b.c.d.e", true},
      {"1.2.3.4.5", true},
      {"1.2.3..4.5", false},
      {"1.2.3.4.5.", true},
      {"1.2.3.4.5..", false},
      {"%20%20noodles.blorg", false},
      {"noo dles.blorg ", false},
      {"noo dles.blorg. ", false},
      {"^noodles.blorg", false},
      {"noodles^.blorg", false},
      {"noo&dles.blorg", false},
      {"noodles.blorg`", false},
      {"www.noodles.blorg", true},
      {"1www.noodles.blorg", true},
      {"www.2noodles.blorg", true},
      {"www.n--oodles.blorg", true},
      {"www.noodl_es.blorg", true},
      {"www.no-_odles.blorg", true},
      {"www_.noodles.blorg", true},
      {"www.noodles.blorg.", true},
      {"_privet._tcp.local", true},
      // 63-char label (before or without dot) allowed
      {"z23456789a123456789a123456789a123456789a123456789a123456789a123", true},
      {"z23456789a123456789a123456789a123456789a123456789a123456789a123.",
       true},
      // 64-char label (before or without dot) disallowed
      {"123456789a123456789a123456789a123456789a123456789a123456789a1234",
       false},
      {"123456789a123456789a123456789a123456789a123456789a123456789a1234.",
       false},
      // 253-char host allowed
      {"abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abc",
       true},
      // 253-char+dot host allowed
      {"abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi.abcdefghi."
       "abcdefghi.abcdefghi.abcdefghi.abcdefghi.abc.",
       true},
      // 254-char host disallowed
      {"123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.1234",
       false},
      // 254-char+dot host disallowed
      {"123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.123456789.123456789.123456789."
       "123456789.123456789.123456789.123456789.1234.",
       false},
  };

  for (const auto& compliant_host : compliant_host_cases) {
    EXPECT_EQ(compliant_host.expected_output,
              IsCanonicalizedHostCompliant(compliant_host.host))
        << compliant_host.host;
  }
}

struct NonUniqueNameTestData {
  bool is_unique;
  const char* const hostname;
};

// Google Test pretty-printer.
void PrintTo(const NonUniqueNameTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname);
  *os << " hostname: " << testing::PrintToString(data.hostname)
      << "; is_unique: " << testing::PrintToString(data.is_unique);
}

const NonUniqueNameTestData kNonUniqueNameTestData[] = {
    // eTLDs
    {true, "com"},
    {true, "com."},
    {true, ".com"},
    {true, "co.uk"},
    {true, "co.uk."},
    {true, ".co.uk"},
    {false, "notarealtld"},
    {false, ".notarealtld"},
    {false, "notarealtld."},
    // Domains under ICANN-assigned domains.
    {true, "google.com"},
    {true, "google.co.uk"},
    // Domains under private registries.
    {true, "appspot.com"},
    {true, "test.appspot.com"},
    // Unreserved IPv4 addresses (in various forms).
    {true, "8.8.8.8"},
    {true, "99.64.0.0"},
    {true, "212.15.0.0"},
    {true, "212.15"},
    {true, "212.15.0"},
    {true, "3557752832"},
    // Reserved IPv4 addresses (in various forms).
    {false, "192.168.0.0"},
    {false, "192.168.0.6"},
    {false, "10.0.0.5"},
    {false, "10.0"},
    {false, "10.0.0"},
    {false, "3232235526"},
    // Unreserved IPv6 addresses.
    {true, "FFC0:ba98:7654:3210:FEDC:BA98:7654:3210"},
    {true, "2000:ba98:7654:2301:EFCD:BA98:7654:3210"},
    // Reserved IPv6 addresses.
    {false, "::192.9.5.5"},
    {false, "FEED::BEEF"},
    {false, "FEC0:ba98:7654:3210:FEDC:BA98:7654:3210"},
    // 'internal'/non-IANA assigned domains.
    {false, "intranet"},
    {false, "intranet."},
    {false, "intranet.example"},
    {false, "host.intranet.example"},
    // gTLDs under discussion, but not yet assigned.
    {false, "intranet.corp"},
    {false, "intranet.internal"},
    // Invalid host names are treated as unique - but expected to be
    // filtered out before then.
    {true, "junk)(£)$*!@~#"},
    {true, "w$w.example.com"},
    {true, "nocolonsallowed:example"},
    {true, "[::4.5.6.9]"},
};

class UrlUtilNonUniqueNameTest
    : public testing::TestWithParam<NonUniqueNameTestData> {
 public:
  ~UrlUtilNonUniqueNameTest() override = default;

 protected:
  bool IsUnique(const std::string& hostname) {
    return !IsHostnameNonUnique(hostname);
  }
};

// Test that internal/non-unique names are properly identified as such, but
// that IP addresses and hosts beneath registry-controlled domains are flagged
// as unique names.
TEST_P(UrlUtilNonUniqueNameTest, IsHostnameNonUnique) {
  const NonUniqueNameTestData& test_data = GetParam();

  EXPECT_EQ(test_data.is_unique, IsUnique(test_data.hostname));
}

INSTANTIATE_TEST_SUITE_P(All,
                         UrlUtilNonUniqueNameTest,
                         testing::ValuesIn(kNonUniqueNameTestData));

TEST(UrlUtilTest, IsLocalhost) {
  EXPECT_TRUE(HostStringIsLocalhost("localhost"));
  EXPECT_TRUE(HostStringIsLocalhost("localHosT"));
  EXPECT_TRUE(HostStringIsLocalhost("localhost."));
  EXPECT_TRUE(HostStringIsLocalhost("localHost."));
  EXPECT_TRUE(HostStringIsLocalhost("127.0.0.1"));
  EXPECT_TRUE(HostStringIsLocalhost("127.0.1.0"));
  EXPECT_TRUE(HostStringIsLocalhost("127.1.0.0"));
  EXPECT_TRUE(HostStringIsLocalhost("127.0.0.255"));
  EXPECT_TRUE(HostStringIsLocalhost("127.0.255.0"));
  EXPECT_TRUE(HostStringIsLocalhost("127.255.0.0"));
  EXPECT_TRUE(HostStringIsLocalhost("::1"));
  EXPECT_TRUE(HostStringIsLocalhost("0:0:0:0:0:0:0:1"));
  EXPECT_TRUE(HostStringIsLocalhost("foo.localhost"));
  EXPECT_TRUE(HostStringIsLocalhost("foo.localhost."));
  EXPECT_TRUE(HostStringIsLocalhost("foo.localhoST"));
  EXPECT_TRUE(HostStringIsLocalhost("foo.localhoST."));

  EXPECT_FALSE(HostStringIsLocalhost("localhost.localdomain"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost.localDOMain"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost.localdomain."));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6."));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6.localdomain6"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6.localdomain6."));

  EXPECT_FALSE(HostStringIsLocalhost("localhostx"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost.x"));
  EXPECT_FALSE(HostStringIsLocalhost("foo.localdomain"));
  EXPECT_FALSE(HostStringIsLocalhost("foo.localdomain.x"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6x"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(HostStringIsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(HostStringIsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(HostStringIsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(HostStringIsLocalhost("::2"));
  EXPECT_FALSE(HostStringIsLocalhost("::1:1"));
  EXPECT_FALSE(HostStringIsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(HostStringIsLocalhost("::1:1"));
  EXPECT_FALSE(HostStringIsLocalhost("0:0:0:0:0:0:0:0:1"));
  EXPECT_FALSE(HostStringIsLocalhost("foo.localhost.com"));
  EXPECT_FALSE(HostStringIsLocalhost("foo.localhoste"));
  EXPECT_FALSE(HostStringIsLocalhost("foo.localhos"));
  EXPECT_FALSE(HostStringIsLocalhost("[::1]"));

  GURL localhost6("http://[::1]/");
  EXPECT_TRUE(IsLocalhost(localhost6));
}

class UrlUtilTypedTest : public ::testing::TestWithParam<bool> {
 public:
  UrlUtilTypedTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      scoped_feature_list_.InitAndEnableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          url::kStandardCompliantNonSpecialSchemeURLParsing);
    }
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, UrlUtilTypedTest, ::testing::Bool());

TEST(UrlUtilTest, SimplifyUrlForRequest) {
  struct {
    const char* const input_url;
    const char* const expected_simplified_url;
  } tests[] = {
    {
      // Reference section should be stripped.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
    },
    {
      // Reference section can itself contain #.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1?query=1",
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
    },
    { // Strip both the reference and the username/password.
      "http://user:pass@google.com:80/sup?yo#X#X",
      "http://google.com/sup?yo",
    },
    { // Try an HTTPS URL -- strip both the reference and the username/password.
      "https://user:pass@google.com:80/sup?yo#X#X",
      "https://google.com:80/sup?yo",
    },
    { // Try an FTP URL -- strip both the reference and the username/password.
      "ftp://user:pass@google.com:80/sup?yo#X#X",
      "ftp://google.com:80/sup?yo",
    },
  };
  for (const auto& test : tests) {
    SCOPED_TRACE(test.input_url);
    GURL input_url(GURL(test.input_url));
    GURL expected_url(GURL(test.expected_simplified_url));
    EXPECT_EQ(expected_url, SimplifyUrlForRequest(input_url));
  }
}

TEST_P(UrlUtilTypedTest, SimplifyUrlForRequest) {
  static constexpr struct {
    const char* const input_url;
    const char* const expected_when_compliant;
    const char* const expected_when_non_compliant;
  } tests[] = {
      {
          // Try a non-special URL
          "foobar://user:pass@google.com:80/sup?yo#X#X",
          "foobar://google.com:80/sup?yo",
          "foobar://user:pass@google.com:80/sup?yo",
      },
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.input_url);
    GURL simplified = SimplifyUrlForRequest(GURL(test.input_url));
    if (use_standard_compliant_non_special_scheme_url_parsing_) {
      EXPECT_EQ(simplified, GURL(test.expected_when_compliant));
    } else {
      EXPECT_EQ(simplified, GURL(test.expected_when_non_compliant));
    }
  }
}

TEST(UrlUtilTest, ChangeWebSocketSchemeToHttpScheme) {
  struct {
    const char* const input_url;
    const char* const expected_output_url;
  } tests[] = {
      {"ws://google.com:78/path?query=1", "http://google.com:78/path?query=1"},
      {"wss://google.com:441/path?q=1", "https://google.com:441/path?q=1"}};
  for (const auto& test : tests) {
    GURL input_url(test.input_url);
    GURL expected_output_url(test.expected_output_url);
    EXPECT_EQ(expected_output_url,
              ChangeWebSocketSchemeToHttpScheme(input_url));
  }
}

TEST(UrlUtilTest, SchemeHasNetworkHost) {
  const char kCustomSchemeWithHostPortAndUserInformation[] = "foo";
  const char kCustomSchemeWithHostAndPort[] = "bar";
  const char kCustomSchemeWithHost[] = "baz";
  const char kCustomSchemeWithoutAuthority[] = "qux";
  const char kNonStandardScheme[] = "not-registered";

  url::ScopedSchemeRegistryForTests scheme_registry;
  AddStandardScheme(kCustomSchemeWithHostPortAndUserInformation,
                    url::SCHEME_WITH_HOST_PORT_AND_USER_INFORMATION);
  AddStandardScheme(kCustomSchemeWithHostAndPort,
                    url::SCHEME_WITH_HOST_AND_PORT);
  AddStandardScheme(kCustomSchemeWithHost, url::SCHEME_WITH_HOST);
  AddStandardScheme(kCustomSchemeWithoutAuthority,
                    url::SCHEME_WITHOUT_AUTHORITY);

  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kHttpScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kHttpsScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kWsScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kWssScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kFtpScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(url::kFileScheme));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(
      kCustomSchemeWithHostPortAndUserInformation));
  EXPECT_TRUE(IsStandardSchemeWithNetworkHost(kCustomSchemeWithHostAndPort));

  EXPECT_FALSE(IsStandardSchemeWithNetworkHost(url::kFileSystemScheme));
  EXPECT_FALSE(IsStandardSchemeWithNetworkHost(kCustomSchemeWithHost));
  EXPECT_FALSE(IsStandardSchemeWithNetworkHost(kCustomSchemeWithoutAuthority));
  EXPECT_FALSE(IsStandardSchemeWithNetworkHost(kNonStandardScheme));
}

TEST(UrlUtilTest, GetIdentityFromURL) {
  struct {
    const char* const input_url;
    const char* const expected_username;
    const char* const expected_password;
  } tests[] = {
      {
          "http://username:password@google.com",
          "username",
          "password",
      },
      {
          // Test for http://crbug.com/19200
          "http://username:p@ssword@google.com",
          "username",
          "p@ssword",
      },
      {
          // Special URL characters should be unescaped.
          "http://username:p%3fa%26s%2fs%23@google.com",
          "username",
          "p?a&s/s#",
      },
      {
          // Username contains %20, password %25.
          "http://use rname:password%25@google.com",
          "use rname",
          "password%",
      },
      {
          // Username and password contain forward / backward slashes.
          "http://username%2F:password%5C@google.com",
          "username/",
          "password\\",
      },
      {
          // Keep %00 and %01 as-is, and ignore other escaped characters when
          // present.
          "http://use%00rname%20:pass%01word%25@google.com",
          "use%00rname%20",
          "pass%01word%25",
      },
      {
          // Keep CR and LF as-is.
          "http://use%0Arname:pass%0Dword@google.com",
          "use%0Arname",
          "pass%0Dword",
      },
      {
          // Use a '+' in the username.
          "http://use+rname:password@google.com",
          "use+rname",
          "password",
      },
      {
          // Use a '&' in the password.
          "http://username:p&ssword@google.com",
          "username",
          "p&ssword",
      },
      {
          // These UTF-8 characters are considered unsafe to unescape by
          // UnescapeURLComponent, but raise no special concerns as part of the
          // identity portion of a URL.
          "http://%F0%9F%94%92:%E2%80%82@google.com",
          "\xF0\x9F\x94\x92",
          "\xE2\x80\x82",
      },
      {
          // Leave invalid UTF-8 alone, and leave valid UTF-8 characters alone
          // if there's also an invalid character in the string - strings should
          // not be partially unescaped.
          "http://%81:%E2%80%82%E2%80@google.com",
          "%81",
          "%E2%80%82%E2%80",
      },
  };
  for (const auto& test : tests) {
    SCOPED_TRACE(test.input_url);
    GURL url(test.input_url);

    std::u16string username, password;
    GetIdentityFromURL(url, &username, &password);

    EXPECT_EQ(base::UTF8ToUTF16(test.expected_username), username);
    EXPECT_EQ(base::UTF8ToUTF16(test.expected_password), password);
  }
}

// Try extracting a username which was encoded with UTF8.
TEST(UrlUtilTest, GetIdentityFromURL_UTF8) {
  GURL url(u"http://foo:\x4f60\x597d@blah.com");

  EXPECT_EQ("foo", url.username());
  EXPECT_EQ("%E4%BD%A0%E5%A5%BD", url.password());

  // Extract the unescaped identity.
  std::u16string username, password;
  GetIdentityFromURL(url, &username, &password);

  // Verify that it was decoded as UTF8.
  EXPECT_EQ(u"foo", username);
  EXPECT_EQ(u"\x4f60\x597d", password);
}

TEST(UrlUtilTest, GoogleHost) {
  struct {
    GURL url;
    bool expected_output;
  } google_host_cases[] = {
      {GURL("http://.google.com"), true},
      {GURL("http://.youtube.com"), true},
      {GURL("http://.gmail.com"), true},
      {GURL("http://.doubleclick.net"), true},
      {GURL("http://.gstatic.com"), true},
      {GURL("http://.googlevideo.com"), true},
      {GURL("http://.googleusercontent.com"), true},
      {GURL("http://.googlesyndication.com"), true},
      {GURL("http://.google-analytics.com"), true},
      {GURL("http://.googleadservices.com"), true},
      {GURL("http://.googleapis.com"), true},
      {GURL("http://a.google.com"), true},
      {GURL("http://b.youtube.com"), true},
      {GURL("http://c.gmail.com"), true},
      {GURL("http://google.com"), false},
      {GURL("http://youtube.com"), false},
      {GURL("http://gmail.com"), false},
      {GURL("http://google.coma"), false},
      {GURL("http://agoogle.com"), false},
      {GURL("http://oogle.com"), false},
      {GURL("http://google.co"), false},
      {GURL("http://oggole.com"), false},
  };

  for (const auto& host : google_host_cases) {
    EXPECT_EQ(host.expected_output, HasGoogleHost(host.url));
  }
}

TEST(UrlUtilTest, IsLocalHostname) {
  EXPECT_TRUE(IsLocalHostname("localhost"));
  EXPECT_TRUE(IsLocalHostname("localhost."));
  EXPECT_TRUE(IsLocalHostname("LOCALhost"));
  EXPECT_TRUE(IsLocalHostname("LOCALhost."));
  EXPECT_TRUE(IsLocalHostname("abc.localhost"));
  EXPECT_TRUE(IsLocalHostname("abc.localhost."));
  EXPECT_TRUE(IsLocalHostname("abc.LOCALhost"));
  EXPECT_TRUE(IsLocalHostname("abc.LOCALhost."));
  EXPECT_TRUE(IsLocalHostname("abc.def.localhost"));

  EXPECT_FALSE(IsLocalHostname("localhost.actuallynot"));
  EXPECT_FALSE(IsLocalHostname("notlocalhost"));
  EXPECT_FALSE(IsLocalHostname("notlocalhost."));
  EXPECT_FALSE(IsLocalHostname("still.notlocalhost"));
  EXPECT_FALSE(IsLocalHostname("localhostjustkidding"));
}

TEST(UrlUtilTest, GoogleHostWithAlpnH3) {
  struct {
    std::string_view host;
    bool expected_output;
  } test_cases[] = {
      {"google.com", true},        {"www.google.com", true},
      {"google.CoM", true},        {"www.Google.cOm", true},
      {"www.google.cat", false},   {"www.google.co.in", false},
      {"www.google.co.jp", false},
  };

  for (const auto& host : test_cases) {
    EXPECT_EQ(host.expected_output, IsGoogleHostWithAlpnH3(host.host));
  }
}

}  // namespace
}  // namespace net

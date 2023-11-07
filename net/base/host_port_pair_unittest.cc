// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include <optional>

#include "base/values.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

using std::string;
using testing::Optional;

namespace net {

namespace {

struct TestData {
  string host;
  uint16_t port;
  string to_string;
  string host_for_url;
} tests[] = {
  { "www.google.com", 80, "www.google.com:80", "www.google.com" },
  { "www.google.com", 443, "www.google.com:443", "www.google.com" },
  { "127.0.0.1", 80, "127.0.0.1:80", "127.0.0.1" },
  { "192.168.1.1", 80, "192.168.1.1:80", "192.168.1.1" },
  { "::1", 80, "[::1]:80", "[::1]" },
  { "2001:db8::42", 80, "[2001:db8::42]:80", "[2001:db8::42]" },
};

TEST(HostPortPairTest, Parsing) {
  HostPortPair foo("foo.com", 10);
  string foo_str = foo.ToString();
  EXPECT_EQ("foo.com:10", foo_str);
  HostPortPair bar = HostPortPair::FromString(foo_str);
  EXPECT_TRUE(foo.Equals(bar));
}

TEST(HostPortPairTest, ParsingIpv6) {
  HostPortPair foo("2001:db8::42", 100);
  string foo_str = foo.ToString();
  EXPECT_EQ("[2001:db8::42]:100", foo_str);
  HostPortPair bar = HostPortPair::FromString(foo_str);
  EXPECT_TRUE(foo.Equals(bar));
}

TEST(HostPortPairTest, BadString) {
  const char* kBadStrings[] = {"foo.com",           "foo.com:",
                               "foo.com:2:3",       "bar.com:two",
                               "www.google.com:-1", "www.google.com:+1",
                               "127.0.0.1:65536",   "[2001:db8::42]:65536",
                               "[2001:db8::42",     "2001:db8::42",
                               "2001:db8::42:100",  "[2001:db8::42]"};

  for (const auto* const test : kBadStrings) {
    SCOPED_TRACE(test);
    HostPortPair foo = HostPortPair::FromString(test);
    EXPECT_TRUE(foo.host().empty());
    EXPECT_EQ(0, foo.port());
  }
}

TEST(HostPortPairTest, Emptiness) {
  HostPortPair foo;
  EXPECT_TRUE(foo.IsEmpty());
  foo = HostPortPair::FromString("foo.com:8080");
  EXPECT_FALSE(foo.IsEmpty());
}

TEST(HostPortPairTest, ToString) {
  for (const auto& test : tests) {
    HostPortPair foo(test.host, test.port);
    EXPECT_EQ(test.to_string, foo.ToString());
  }

  // Test empty hostname.
  HostPortPair foo(string(), 10);
}

TEST(HostPortPairTest, HostForURL) {
  for (const auto& test : tests) {
    HostPortPair foo(test.host, test.port);
    EXPECT_EQ(test.host_for_url, foo.HostForURL());
  }

  // Test hostname with null character.
  string bar_hostname("a\0.\0com", 7);
  HostPortPair bar(bar_hostname, 80);
  string expected_error("Host has a null char: a%00.%00com");
  EXPECT_DFATAL(bar.HostForURL(), expected_error);
}

TEST(HostPortPairTest, LessThan) {
  HostPortPair a_10("a.com", 10);
  HostPortPair a_11("a.com", 11);
  HostPortPair b_10("b.com", 10);
  HostPortPair b_11("b.com", 11);

  EXPECT_FALSE(a_10 < a_10);
  EXPECT_TRUE(a_10  < a_11);
  EXPECT_TRUE(a_10  < b_10);
  EXPECT_TRUE(a_10  < b_11);

  EXPECT_FALSE(a_11 < a_10);
  EXPECT_FALSE(a_11 < b_10);

  EXPECT_FALSE(b_10 < a_10);
  EXPECT_TRUE(b_10  < a_11);

  EXPECT_FALSE(b_11 < a_10);
}

TEST(HostPortPairTest, Equals) {
  HostPortPair a_10("a.com", 10);
  HostPortPair a_11("a.com", 11);
  HostPortPair b_10("b.com", 10);
  HostPortPair b_11("b.com", 11);

  HostPortPair new_a_10("a.com", 10);

  EXPECT_TRUE(new_a_10.Equals(a_10));
  EXPECT_FALSE(new_a_10.Equals(a_11));
  EXPECT_FALSE(new_a_10.Equals(b_10));
  EXPECT_FALSE(new_a_10.Equals(b_11));
}

TEST(HostPortPairTest, ParsesFromUrl) {
  HostPortPair parsed = HostPortPair::FromURL(GURL("https://foo.test:1250"));
  HostPortPair expected("foo.test", 1250);

  EXPECT_EQ(parsed, expected);
}

TEST(HostPortPairTest, ParsesFromUrlWithIpv6Brackets) {
  HostPortPair parsed = HostPortPair::FromURL(GURL("https://[::1]"));
  HostPortPair expected("::1", 443);

  EXPECT_EQ(parsed, expected);
}

TEST(HostPortPairTest, ParsesFromSchemeHostPort) {
  HostPortPair parsed = HostPortPair::FromSchemeHostPort(
      url::SchemeHostPort("ws", "bar.test", 111));
  HostPortPair expected("bar.test", 111);

  EXPECT_EQ(parsed, expected);
}

TEST(HostPortPairTest, ParsesFromSchemeHostPortWithIpv6Brackets) {
  HostPortPair parsed = HostPortPair::FromSchemeHostPort(
      url::SchemeHostPort("wss", "[::1022]", 112));
  HostPortPair expected("::1022", 112);

  EXPECT_EQ(parsed, expected);
}

TEST(HostPortPairTest, RoundtripThroughValue) {
  HostPortPair pair("foo.test", 1456);
  base::Value value = pair.ToValue();

  EXPECT_THAT(HostPortPair::FromValue(value), Optional(pair));
}

TEST(HostPortPairTest, DeserializeGarbageValue) {
  base::Value value(43);
  EXPECT_FALSE(HostPortPair::FromValue(value).has_value());
}

TEST(HostPortPairTest, DeserializeMalformedValues) {
  base::Value valid_value = HostPortPair("foo.test", 123).ToValue();
  ASSERT_TRUE(HostPortPair::FromValue(valid_value).has_value());

  base::Value missing_host = valid_value.Clone();
  ASSERT_TRUE(missing_host.GetDict().Remove("host"));
  EXPECT_FALSE(HostPortPair::FromValue(missing_host).has_value());

  base::Value missing_port = valid_value.Clone();
  ASSERT_TRUE(missing_port.GetDict().Remove("port"));
  EXPECT_FALSE(HostPortPair::FromValue(missing_port).has_value());

  base::Value negative_port = valid_value.Clone();
  *negative_port.GetDict().Find("port") = base::Value(-1);
  EXPECT_FALSE(HostPortPair::FromValue(negative_port).has_value());

  base::Value large_port = valid_value.Clone();
  *large_port.GetDict().Find("port") = base::Value(66000);
  EXPECT_FALSE(HostPortPair::FromValue(large_port).has_value());
}

}  // namespace

}  // namespace net

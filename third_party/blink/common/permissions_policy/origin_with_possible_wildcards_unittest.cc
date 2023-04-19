// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

TEST(OriginWithPossibleWildcardsTest, DoesMatchOrigin) {
  // Tuple of {origin to test, origin in policy, w/ wildcard, result,
  // description}.
  const auto& values = {
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), false, true,
                      "Same origin, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("http://foo.com")), false, false,
                      "Different scheme, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://foo.com:443")), false,
                      true, "Ignore default port, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), false,
                      false, "Subdomain matches, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://bar.foo.com")), false,
                      false, "Different subdomain, no wildcard"),
      std::make_tuple(url::Origin(),
                      url::Origin::Create(GURL("https://foo.com")), false,
                      false, "Opaque to origin, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("file:///test")),
                      url::Origin::Create(GURL("file:///test")), false, true,
                      "File, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      url::Origin::Create(GURL("http://192.168.1.1")), false,
                      true, "Same IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      url::Origin::Create(GURL("http://192.168.1.2")), false,
                      false, "Different IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://[2001:db8::1]")),
                      url::Origin::Create(GURL("http://[2001:db8::1]")), false,
                      true, "Same IPv6, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://[2001:db8::1]")),
                      url::Origin::Create(GURL("http://[2001:db8::2]")), false,
                      false, "Different IPv6, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true, false,
                      "Same origin, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true, true,
                      "Subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true, false,
                      "Different scheme, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://baz.bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true, true,
                      "Sub-subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://bar.foo.com")), true,
                      false, "Subdomain doesn't match, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com:443")), true,
                      true, "Ignore default port, w/ wildcard"),
      std::make_tuple(url::Origin(),
                      url::Origin::Create(GURL("https://foo.com")), true, false,
                      "Opaque to origin, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("file:///test")),
                      url::Origin::Create(GURL("file:///test")), true, true,
                      "File, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      url::Origin::Create(GURL("http://192.168.1.1")), true,
                      false, "Same IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      url::Origin::Create(GURL("http://192.168.1.2")), true,
                      false, "Different IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://[2001:db8::1]")),
                      url::Origin::Create(GURL("http://[2001:db8::1]")), true,
                      false, "Same IPv6, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://[2001:db8::1]")),
                      url::Origin::Create(GURL("http://[2001:db8::2]")), true,
                      false, "Different IPv6, no wildcard")};
  for (const auto& value : values) {
    SCOPED_TRACE(std::get<4>(value));
    EXPECT_EQ(std::get<3>(value), OriginWithPossibleWildcards(
                                      std::get<1>(value), std::get<2>(value))
                                      .DoesMatchOrigin(std::get<0>(value)));
  }
}

TEST(OriginWithPossibleWildcardsTest, Parse) {
  // Tuple of {serialized value, type, scheme, host, port, wildcard,
  // description}.
  const auto& values = {
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", -1, false,
                      "Origin without subdomain wildcard in header"),
      std::make_tuple("http://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "http",
                      "foo.com", -1, false,
                      "Insecure origin without subdomain wildcard in header"),
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", -1, false,
                      "Origin without subdomain wildcard in attribute"),
      std::make_tuple(
          "http://foo.com", OriginWithPossibleWildcards::NodeType::kAttribute,
          "http", "foo.com", -1, false,
          "Insecure origin without subdomain wildcard in attribute"),
      std::make_tuple("https://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", -1, true,
                      "Origin with subdomain wildcard in header"),
      std::make_tuple("http://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "http",
                      "foo.com", -1, true,
                      "Insecure origin with subdomain wildcard in header"),
      std::make_tuple("https://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "Origin with subdomain wildcard in attribute"),
      std::make_tuple("http://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false,
                      "Insecure origin with subdomain wildcard in attribute"),
      std::make_tuple("*://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "Origin with scheme wildcard in header"),
      std::make_tuple("*://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "Origin with scheme wildcard in attribute"),
      std::make_tuple("https://*",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "Origin with host wildcard in header"),
      std::make_tuple("https://*",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "Origin with host wildcard in attribute"),
      std::make_tuple(
          "https://*.com", OriginWithPossibleWildcards::NodeType::kHeader, "",
          "", 0, false, "Origin with non-registerable host wildcard in header"),
      std::make_tuple(
          "https://*.com", OriginWithPossibleWildcards::NodeType::kAttribute,
          "", "", 0, false,
          "Origin with non-registerable host wildcard in attribute"),
      std::make_tuple("https://*.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false,
                      "Origin with only private tld host wildcard in header"),
      std::make_tuple(
          "https://*.appspot.com",
          OriginWithPossibleWildcards::NodeType::kAttribute, "", "", 0, false,
          "Origin with only private tld host wildcard in attribute"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.appspot.com", -1, true,
                      "Origin with private tld host wildcard in header"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false,
                      "Origin with private tld host wildcard in attribute"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "example.test", -1, true,
                      "Origin with unknown tld host wildcard in header"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false,
                      "Origin with unknown tld host wildcard in attribute"),
      std::make_tuple(
          "https://foo.com:443", OriginWithPossibleWildcards::NodeType::kHeader,
          "https", "foo.com", 443, false, "Origin with default port in header"),
      std::make_tuple("https://foo.com:443",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", 443, false,
                      "Origin with default port in attribute"),
      std::make_tuple(
          "https://foo.com:444", OriginWithPossibleWildcards::NodeType::kHeader,
          "https", "foo.com", 444, false, "Origin with custom port in header"),
      std::make_tuple("https://foo.com:444",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", 444, false,
                      "Origin with custom port in attribute"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "Origin with port wildcard in header"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "Origin with port wildcard in attribute"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false,
                      "Origin with improper subdomain wildcard in header"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false,
                      "Origin with improper subdomain wildcard in attribute"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "Origin with two subdomain wildcards in header"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false,
                      "Origin with two subdomain wildcards in attribute"),
      std::make_tuple("https://192.168.0.1",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "192.168.0.1", -1, false, "IPv4 Address in header"),
      std::make_tuple("https://192.168.0.1",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "192.168.0.1", -1, false,
                      "IPv4 Address in attribute"),
      std::make_tuple("https://192.*.0.1",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "IPv4 Address w/ wildcard in header"),
      std::make_tuple("https://192.*.0.1",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "IPv4 Address w/ wildcard in attribute"),
      std::make_tuple("https://[2001:db8::1]",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "IPv6 Address in header"),
      std::make_tuple("https://[2001:db8::1]",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "IPv6 Address in attribute"),
      std::make_tuple("file://example.com/test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "file",
                      "example.com", -1, false, "File Host in header"),
      std::make_tuple("file://example.com/test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "file",
                      "example.com", -1, false, "File Host in attribute"),
      std::make_tuple("file://*.example.com/test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "file",
                      "example.com", -1, true,
                      "File Host w/ wildcard in header"),
      std::make_tuple("file://*.example.com/test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "File Host w/ wildcard in attribute"),
      std::make_tuple("file:///test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, "File Path in header"),
      std::make_tuple("file:///test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, "File Path in attribute"),
  };
  for (const auto& value : values) {
    const auto& origin_with_possible_wildcards =
        OriginWithPossibleWildcards::Parse(std::get<0>(value),
                                           std::get<1>(value));
    SCOPED_TRACE(std::get<6>(value));
    if (strlen(std::get<2>(value))) {
      EXPECT_EQ(std::get<2>(value),
                origin_with_possible_wildcards->csp_source.scheme);
      EXPECT_EQ(std::get<3>(value),
                origin_with_possible_wildcards->csp_source.host);
      EXPECT_EQ(std::get<4>(value),
                origin_with_possible_wildcards->csp_source.port);
      EXPECT_EQ("", origin_with_possible_wildcards->csp_source.path);
      EXPECT_EQ(std::get<5>(value),
                origin_with_possible_wildcards->csp_source.is_host_wildcard);
      EXPECT_FALSE(origin_with_possible_wildcards->csp_source.is_port_wildcard);
    } else {
      EXPECT_FALSE(origin_with_possible_wildcards);
    }
  }
}

TEST(OriginWithPossibleWildcardsTest, Serialize) {
  // Tuple of {origin, wildcard, serialized value, description}.
  const auto& values = {
      std::make_tuple("https://foo.com", false, "https://foo.com",
                      "Origin without subdomain wildcard"),
      std::make_tuple("https://foo.com", true, "https://*.foo.com",
                      "Origin with subdomain wildcard"),
      std::make_tuple("https://%2A.foo.com", false, "https://%2A.foo.com",
                      "Origin with improper subdomain wildcard"),
      std::make_tuple("https://%2A.com", false, "https://%2A.com",
                      "Origin with non-registerable subdomain wildcard"),
  };
  for (const auto& value : values) {
    const auto& origin_with_possible_wildcards = OriginWithPossibleWildcards(
        url::Origin::Create(GURL(std::get<0>(value))), std::get<1>(value));
    SCOPED_TRACE(std::get<3>(value));
    EXPECT_EQ(std::get<2>(value), origin_with_possible_wildcards.Serialize());
  }
}

TEST(OriginWithPossibleWildcardsTest, Mojom) {
  // Tuple of {origin, wildcard, description}.
  const auto& values = {
      std::make_tuple("https://foo.com", false,
                      "Origin without subdomain wildcard"),
      std::make_tuple("https://foo.com", true,
                      "Origin with subdomain wildcard"),
      std::make_tuple("https://192.168.0.1", false,
                      "IPv4 without subdomain wildcard"),
      std::make_tuple("https://192.168.0.1", true,
                      "IPv4 with subdomain wildcard"),
      std::make_tuple("https://[2001:db8::1]", false,
                      "IPv6 without subdomain wildcard"),
      std::make_tuple("https://[2001:db8::1]", true,
                      "IPv6 with subdomain wildcard"),
      std::make_tuple("file://example.com/test", false,
                      "File host without subdomain wildcard"),
      std::make_tuple("file://example.com/test", true,
                      "File host with subdomain wildcard"),
      std::make_tuple("file:///test", false,
                      "File path without subdomain wildcard"),
      std::make_tuple("file:///test", true,
                      "File path with subdomain wildcard"),
  };
  for (const auto& value : values) {
    SCOPED_TRACE(std::get<2>(value));
    const auto& original = OriginWithPossibleWildcards(
        url::Origin::Create(GURL(std::get<0>(value))), std::get<1>(value));
    OriginWithPossibleWildcards copy;
    EXPECT_NE(original, copy);
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(
            original, copy));
    EXPECT_EQ(original, copy);
  }
}

TEST(OriginWithPossibleWildcardsTest, DefaultPorts) {
  OriginWithPossibleWildcards a(url::Origin::Create(GURL("https://google.com")),
                                false);
  OriginWithPossibleWildcards b(
      url::Origin::Create(GURL("https://google.com:443")), false);
  OriginWithPossibleWildcards c(url::Origin::Create(GURL("https://google.com")),
                                true);
  OriginWithPossibleWildcards d = c;
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_EQ(c, d);
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(
          a, c));
  EXPECT_EQ(a, c);
}

TEST(OriginWithPossibleWildcardsTest, Opaque) {
  EXPECT_DCHECK_DEATH(OriginWithPossibleWildcards(url::Origin(), true));
  EXPECT_DCHECK_DEATH(OriginWithPossibleWildcards(url::Origin(), false));
  OriginWithPossibleWildcards original;
  OriginWithPossibleWildcards copy;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(
          original, copy));
}

}  // namespace blink

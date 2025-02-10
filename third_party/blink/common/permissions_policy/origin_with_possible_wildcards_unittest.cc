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
  // Tuple of {origin to test, serialized value, should parse, should match,
  // description}.
  const auto& values = {
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://foo.com", true, true,
                      "Same origin, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "http://foo.com", true, false,
                      "Different scheme, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://foo.com:443", true, true,
                      "Ignore default port, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      "https://foo.com", true, false,
                      "Subdomain matches, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://bar.foo.com", true, false,
                      "Different subdomain, no wildcard"),
      std::make_tuple(url::Origin(), "https://foo.com", true, false,
                      "Opaque to origin, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("file:///test")),
                      "file://example.com", true, false, "File, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      "http://192.168.1.1", true, true,
                      "Same IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://192.168.1.1")),
                      "http://192.168.1.2", true, false,
                      "Different IPv4, no wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://*.foo.com", true, false,
                      "Same origin, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      "https://*.foo.com", true, true,
                      "Subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://bar.foo.com")),
                      "https://*.foo.com", true, false,
                      "Different scheme, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://baz.bar.foo.com")),
                      "https://*.foo.com", true, true,
                      "Sub-subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://*.bar.foo.com", true, false,
                      "Subdomain doesn't match, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      "https://*.foo.com:443", true, true,
                      "Ignore default port, w/ wildcard"),
      std::make_tuple(url::Origin(), "https://*.foo.com", true, false,
                      "Opaque to origin, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://foo.com:*", true, true, "Wildcard port match"),
      std::make_tuple(url::Origin::Create(GURL("http://foo.com")),
                      "https://foo.com:*", true, false,
                      "Wildcard port mismatch scheme"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")), "https://*",
                      true, true, "Wildcard host match"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://*:80", true, false,
                      "Wildcard host mismatch port"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https://*:*", true, true,
                      "Wildcard host and port match"),
      std::make_tuple(url::Origin::Create(GURL("http://foo.com")),
                      "https://*:*", true, false,
                      "Wildcard host and port mismatch scheme"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      "https:", true, true, "Scheme only match"),
  };
  for (const auto& value : values) {
    SCOPED_TRACE(std::get<4>(value));
    const auto& origin_with_possible_wildcards =
        OriginWithPossibleWildcards::Parse(
            std::get<1>(value), OriginWithPossibleWildcards::NodeType::kHeader);
    if (std::get<2>(value)) {
      EXPECT_EQ(
          std::get<3>(value),
          origin_with_possible_wildcards->DoesMatchOrigin(std::get<0>(value)));
    } else {
      EXPECT_FALSE(origin_with_possible_wildcards);
    }
  }
}

TEST(OriginWithPossibleWildcardsTest, Parse) {
  // Tuple of {serialized value, type, scheme, host, port, host_wildcard,
  // port_wildcard, should parse, description}.
  const auto& values = {
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", -1, false, false, true,
                      "Origin without subdomain wildcard in header"),
      std::make_tuple("http://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "http",
                      "foo.com", -1, false, false, true,
                      "Insecure origin without subdomain wildcard in header"),
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", -1, false, false, true,
                      "Origin without subdomain wildcard in attribute"),
      std::make_tuple(
          "http://foo.com", OriginWithPossibleWildcards::NodeType::kAttribute,
          "http", "foo.com", -1, false, false, true,
          "Insecure origin without subdomain wildcard in attribute"),
      std::make_tuple("https://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", -1, true, false, true,
                      "Origin with subdomain wildcard in header"),
      std::make_tuple("http://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "http",
                      "foo.com", -1, true, false, true,
                      "Insecure origin with subdomain wildcard in header"),
      std::make_tuple("https://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with subdomain wildcard in attribute"),
      std::make_tuple("http://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Insecure origin with subdomain wildcard in attribute"),
      std::make_tuple(
          "*://foo.com", OriginWithPossibleWildcards::NodeType::kHeader, "", "",
          0, false, false, false, "Origin with scheme wildcard in header"),
      std::make_tuple("*://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with scheme wildcard in attribute"),
      std::make_tuple(
          "https://*", OriginWithPossibleWildcards::NodeType::kHeader, "https",
          "", -1, true, false, true, "Origin with host wildcard in header"),
      std::make_tuple(
          "https://*", OriginWithPossibleWildcards::NodeType::kAttribute, "",
          "", 0, false, false, false, "Origin with host wildcard in attribute"),
      std::make_tuple("https://*.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "com", -1, true, false, true,
                      "Origin with non-registerable host wildcard in header"),
      std::make_tuple(
          "https://*.com", OriginWithPossibleWildcards::NodeType::kAttribute,
          "", "", 0, false, false, false,
          "Origin with non-registerable host wildcard in attribute"),
      std::make_tuple("https://*.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "appspot.com", -1, true, false, true,
                      "Origin with only private tld host wildcard in header"),
      std::make_tuple(
          "https://*.appspot.com",
          OriginWithPossibleWildcards::NodeType::kAttribute, "", "", 0, false,
          false, false,
          "Origin with only private tld host wildcard in attribute"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.appspot.com", -1, true, false, true,
                      "Origin with private tld host wildcard in header"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with private tld host wildcard in attribute"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "example.test", -1, true, false, true,
                      "Origin with unknown tld host wildcard in header"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with unknown tld host wildcard in attribute"),
      std::make_tuple("https://foo.com:443",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", 443, false, false, true,
                      "Origin with default port in header"),
      std::make_tuple("https://foo.com:443",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", 443, false, false, true,
                      "Origin with default port in attribute"),
      std::make_tuple("https://foo.com:444",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", 444, false, false, true,
                      "Origin with custom port in header"),
      std::make_tuple("https://foo.com:444",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "foo.com", 444, false, false, true,
                      "Origin with custom port in attribute"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "foo.com", -1, false, true, true,
                      "Origin with port wildcard in header"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with port wildcard in attribute"),
      std::make_tuple("https:", OriginWithPossibleWildcards::NodeType::kHeader,
                      "https", "", -1, false, false, true,
                      "Origin with just scheme in header"),
      std::make_tuple(
          "https:", OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
          0, false, false, false, "Origin with just scheme in attribute"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, false, false,
                      "Origin with improper subdomain wildcard in header"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with improper subdomain wildcard in attribute"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, false, false,
                      "Origin with two subdomain wildcards in header"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "Origin with two subdomain wildcards in attribute"),
      std::make_tuple("https://:443",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, false, false, "Origin with empty host in header"),
      std::make_tuple(
          "https://:443", OriginWithPossibleWildcards::NodeType::kAttribute, "",
          "", 0, false, false, false, "Origin with empty host in attribute"),
      std::make_tuple("*:*", OriginWithPossibleWildcards::NodeType::kHeader, "",
                      "", -1, true, true, false,
                      "Origin with all wildcards in header"),
      std::make_tuple("*:*", OriginWithPossibleWildcards::NodeType::kAttribute,
                      "", "", 0, false, false, false,
                      "Origin with all wildcards in attribute"),
      std::make_tuple("https://192.168.0.1",
                      OriginWithPossibleWildcards::NodeType::kHeader, "https",
                      "192.168.0.1", -1, false, false, true,
                      "IPv4 Address in header"),
      std::make_tuple("https://192.168.0.1",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https", "192.168.0.1", -1, false, false, true,
                      "IPv4 Address in attribute"),
      std::make_tuple(
          "https://192.*.0.1", OriginWithPossibleWildcards::NodeType::kHeader,
          "", "", 0, false, false, false, "IPv4 Address w/ wildcard in header"),
      std::make_tuple("https://192.*.0.1",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "IPv4 Address w/ wildcard in attribute"),
      std::make_tuple("https://[2001:db8::1]",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, false, false, "IPv6 Address in header"),
      std::make_tuple("https://[2001:db8::1]",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false, "IPv6 Address in attribute"),
      std::make_tuple("file://example.com/test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "file",
                      "example.com", -1, false, false, true,
                      "File Host in header"),
      std::make_tuple("file://example.com/test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "file",
                      "example.com", -1, false, false, true,
                      "File Host in attribute"),
      std::make_tuple("file://*.example.com/test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "file",
                      "example.com", -1, true, false, true,
                      "File Host w/ wildcard in header"),
      std::make_tuple("file://*.example.com/test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false,
                      "File Host w/ wildcard in attribute"),
      std::make_tuple("file:///test",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", "", 0,
                      false, false, false, "File Path in header"),
      std::make_tuple("file:///test",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "", "",
                      0, false, false, false, "File Path in attribute"),
  };
  for (const auto& value : values) {
    const auto& origin_with_possible_wildcards =
        OriginWithPossibleWildcards::Parse(std::get<0>(value),
                                           std::get<1>(value));
    SCOPED_TRACE(std::get<8>(value));
    if (std::get<7>(value)) {
      EXPECT_EQ(std::get<2>(value),
                origin_with_possible_wildcards->CSPSourceForTest().scheme);
      EXPECT_EQ(std::get<3>(value),
                origin_with_possible_wildcards->CSPSourceForTest().host);
      EXPECT_EQ(std::get<4>(value),
                origin_with_possible_wildcards->CSPSourceForTest().port);
      EXPECT_EQ("", origin_with_possible_wildcards->CSPSourceForTest().path);
      EXPECT_EQ(
          std::get<5>(value),
          origin_with_possible_wildcards->CSPSourceForTest().is_host_wildcard);
      EXPECT_EQ(
          std::get<6>(value),
          origin_with_possible_wildcards->CSPSourceForTest().is_port_wildcard);
    } else {
      EXPECT_FALSE(origin_with_possible_wildcards);
    }
  }
}

TEST(OriginWithPossibleWildcardsTest, SerializeAndMojom) {
  // Tuple of {serialized value, should parse, description}.
  const auto& values = {
      std::make_tuple("https://foo.com", true, "Origin"),
      std::make_tuple("https://foo.com:433", true, "Origin with port"),
      std::make_tuple("https://*.foo.com", true,
                      "Origin with subdomain wildcard"),
      std::make_tuple("https://*", true, "Origin with host wildcard"),
      std::make_tuple("https://foo.com:*", true, "Origin with port wildcard"),
      std::make_tuple("foo.com", false, "Origin with just host"),
      std::make_tuple("https:", true, "Origin with just scheme"),
      std::make_tuple("https://192.168.0.1", true, "IPv4"),
      std::make_tuple("file://example.com", true, "File host"),
      std::make_tuple("https://[2001:db8::1]", false, "IPv6"),
      std::make_tuple("file:///test", false, "File path"),
  };
  for (const auto& value : values) {
    const auto& original = OriginWithPossibleWildcards::Parse(
        std::get<0>(value), OriginWithPossibleWildcards::NodeType::kHeader);
    SCOPED_TRACE(std::get<2>(value));
    if (std::get<1>(value)) {
      OriginWithPossibleWildcards copy;
      EXPECT_NE(*original, copy);
      EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                  mojom::OriginWithPossibleWildcards>(*original, copy));
      EXPECT_EQ(*original, copy);
      EXPECT_EQ(std::get<0>(value), original->Serialize());
    } else {
      EXPECT_FALSE(original.has_value());
    }
  }
}

TEST(OriginWithPossibleWildcardsTest, Opaque) {
  EXPECT_FALSE(OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
      url::Origin(), true));
  EXPECT_FALSE(OriginWithPossibleWildcards::FromOriginAndWildcardsForTest(
      url::Origin(), false));
  OriginWithPossibleWildcards original;
  OriginWithPossibleWildcards copy;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(
          original, copy));
}

}  // namespace blink

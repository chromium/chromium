// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/test/gtest_util.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"
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
                      url::Origin::Create(GURL("file:///test")), true, false,
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
  // Tuple of {serialized value, type, origin, wildcard, description}.
  const auto& values = {
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://foo.com", false,
                      "Origin without subdomain wildcard in header"),
      std::make_tuple("https://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://foo.com", false,
                      "Origin without subdomain wildcard in attribute"),
      std::make_tuple(
          "https://*.foo.com", OriginWithPossibleWildcards::NodeType::kHeader,
          "https://foo.com", true, "Origin with subdomain wildcard in header"),
      std::make_tuple("https://*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://%2A.foo.com", false,
                      "Origin with subdomain wildcard in attribute"),
      std::make_tuple("*://foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", false,
                      "Origin with scheme wildcard in header"),
      std::make_tuple("*://foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "",
                      false, "Origin with scheme wildcard in attribute"),
      std::make_tuple(
          "https://*", OriginWithPossibleWildcards::NodeType::kHeader,
          "https://%2A", false, "Origin with host wildcard in header"),
      std::make_tuple(
          "https://*", OriginWithPossibleWildcards::NodeType::kAttribute,
          "https://%2A", false, "Origin with host wildcard in attribute"),
      std::make_tuple("https://*.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://%2A.com", false,
                      "Origin with non-registerable host wildcard in header"),
      std::make_tuple(
          "https://*.com", OriginWithPossibleWildcards::NodeType::kAttribute,
          "https://%2A.com", false,
          "Origin with non-registerable host wildcard in attribute"),
      std::make_tuple("https://*.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://%2A.appspot.com", false,
                      "Origin with only private tld host wildcard in header"),
      std::make_tuple(
          "https://*.appspot.com",
          OriginWithPossibleWildcards::NodeType::kAttribute,
          "https://%2A.appspot.com", false,
          "Origin with only private tld host wildcard in attribute"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://foo.appspot.com", true,
                      "Origin with private tld host wildcard in header"),
      std::make_tuple("https://*.foo.appspot.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://%2A.foo.appspot.com", false,
                      "Origin with private tld host wildcard in attribute"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://example.test", true,
                      "Origin with unknown tld host wildcard in header"),
      std::make_tuple("https://*.example.test",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://%2A.example.test", false,
                      "Origin with unknown tld host wildcard in attribute"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kHeader, "", false,
                      "Origin with port wildcard in header"),
      std::make_tuple("https://foo.com:*",
                      OriginWithPossibleWildcards::NodeType::kAttribute, "",
                      false, "Origin with port wildcard in attribute"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://bar.%2A.foo.com", false,
                      "Origin with improper subdomain wildcard in header"),
      std::make_tuple("https://bar.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://bar.%2A.foo.com", false,
                      "Origin with improper subdomain wildcard in attribute"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kHeader,
                      "https://%2A.%2A.foo.com", false,
                      "Origin with two subdomain wildcards in header"),
      std::make_tuple("https://*.*.foo.com",
                      OriginWithPossibleWildcards::NodeType::kAttribute,
                      "https://%2A.%2A.foo.com", false,
                      "Origin with two subdomain wildcards in attribute"),
  };
  for (const auto& value : values) {
    const auto& origin_with_possible_wildcards =
        OriginWithPossibleWildcards::Parse(std::get<0>(value),
                                           std::get<1>(value));
    SCOPED_TRACE(std::get<4>(value));
    if (strlen(std::get<2>(value))) {
      EXPECT_EQ(std::get<2>(value),
                origin_with_possible_wildcards->origin.Serialize());
      EXPECT_EQ(std::get<3>(value),
                origin_with_possible_wildcards->has_subdomain_wildcard);
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

TEST(OriginWithPossibleWildcardsTest, Constructors) {
  OriginWithPossibleWildcards a(url::Origin::Create(GURL("https://google.com")),
                                false);
  OriginWithPossibleWildcards b(
      url::Origin::Create(GURL("https://google.com:443")), false);
  OriginWithPossibleWildcards c(url::Origin::Create(GURL("https://google.com")),
                                true);
  OriginWithPossibleWildcards d = c;
  EXPECT_EQ(a, b);
  EXPECT_NE(b, c);
  EXPECT_EQ(c, d);
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<network::mojom::CSPSource>(a, b));
  EXPECT_EQ(a, b);
}

TEST(OriginWithPossibleWildcardsTest, Opaque) {
  EXPECT_DCHECK_DEATH(OriginWithPossibleWildcards(url::Origin(), true));
  EXPECT_DCHECK_DEATH(OriginWithPossibleWildcards(url::Origin(), false));
  OriginWithPossibleWildcards original;
  OriginWithPossibleWildcards copy;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<network::mojom::CSPSource>(
      original, copy));
}

}  // namespace blink

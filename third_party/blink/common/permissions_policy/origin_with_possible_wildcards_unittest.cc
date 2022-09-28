// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/permissions_policy/origin_with_possible_wildcards.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/common/permissions_policy/permissions_policy_mojom_traits.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

class OriginWithPossibleWildcardsTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        features::kWildcardSubdomainsInPermissionsPolicy,
        HasWildcardSubdomainsInPermissionsPolicy());
  }

  bool HasWildcardSubdomainsInPermissionsPolicy() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, OriginWithPossibleWildcardsTest, testing::Bool());

TEST_P(OriginWithPossibleWildcardsTest, DoesMatchOrigin) {
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
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin(), false, false,
                      "Origin to opaque, no wildcard"),
      std::make_tuple(url::Origin(),
                      url::Origin::Create(GURL("https://foo.com")), false,
                      false, "Opaque to origin, no wildcard"),
      std::make_tuple(url::Origin(), url::Origin(), false, false,
                      "Opaque to opaque, no wildcard"),
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
                      url::Origin::Create(GURL("https://foo.com")), true,
                      HasWildcardSubdomainsInPermissionsPolicy(),
                      "Subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("http://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true, false,
                      "Different scheme, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://baz.bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com")), true,
                      HasWildcardSubdomainsInPermissionsPolicy(),
                      "Sub-subdomain matches, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://foo.com")),
                      url::Origin::Create(GURL("https://bar.foo.com")), true,
                      false, "Subdomain doesn't match, w/ wildcard"),
      std::make_tuple(url::Origin::Create(GURL("https://bar.foo.com")),
                      url::Origin::Create(GURL("https://foo.com:443")), true,
                      HasWildcardSubdomainsInPermissionsPolicy(),
                      "Ignore default port, w/ wildcard"),
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

TEST_P(OriginWithPossibleWildcardsTest, Constructors) {
  OriginWithPossibleWildcards a;
  OriginWithPossibleWildcards b(url::Origin(), false);
  OriginWithPossibleWildcards c(b);
  OriginWithPossibleWildcards d = c;
  EXPECT_NE(a, b);
  EXPECT_EQ(b, c);
  EXPECT_EQ(c, d);
  mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(a, b);
  EXPECT_EQ(a, b);
}

TEST_P(OriginWithPossibleWildcardsTest, Opaque) {
  EXPECT_DCHECK_DEATH(OriginWithPossibleWildcards(url::Origin(), true));
  OriginWithPossibleWildcards original(url::Origin(), false);
  original.has_subdomain_wildcard = true;
  OriginWithPossibleWildcards copy;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::OriginWithPossibleWildcards>(
          original, copy));
}

}  // namespace blink

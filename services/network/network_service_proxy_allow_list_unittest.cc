// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_allow_list.h"

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;

struct MatchTest {
  std::string name;
  std::string req;
  std::string top;
  bool matches;
};

}  // namespace

class NetworkServiceProxyAllowListTest : public ::testing::Test {};

TEST_F(NetworkServiceProxyAllowListTest, NotEnabled) {
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListTest, IsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  NetworkServiceProxyAllowList allowList;

  EXPECT_TRUE(allowList.IsEnabled());
  EXPECT_TRUE(allowList.MakeIpProtectionCustomProxyConfig()
                  ->rules.restrict_to_network_service_proxy_allow_list);
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated) {
  NetworkServiceProxyAllowList allowList;
  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allowList.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated_Empty) {
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_AllowListNotPopulated) {
  NetworkServiceProxyAllowList allowList;

  EXPECT_FALSE(allowList.IsPopulated());
  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"),
                                 GURL("http://example2.com")));
}

// Match returns false for an empty top frame URL. This is believed to be
// impossible, so this represents a "fail open" policy if this circumstance can,
// in fact, occur.
TEST_F(NetworkServiceProxyAllowListTest, Matches_TopFrameUrlIsEmpty) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"), GURL()));
}

TEST_F(NetworkServiceProxyAllowListTest, PartitionMapKey) {
  auto PartitionMapKey = &NetworkServiceProxyAllowList::PartitionMapKey;
  EXPECT_EQ(PartitionMapKey("com"), "com");
  EXPECT_EQ(PartitionMapKey("foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("www.tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("foo.co.uk"), "co.uk");
}

class NetworkServiceProxyAllowListMatchTest
    : public testing::TestWithParam<MatchTest> {};

TEST_P(NetworkServiceProxyAllowListMatchTest, Match) {
  NetworkServiceProxyAllowList allowList;
  MaskedDomainList mdl;

  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("acme");
  resourceOwner->add_owned_resources()->set_domain("acme-ra.com");
  resourceOwner->add_owned_resources()->set_domain("acme-rb.co.uk");
  resourceOwner->add_owned_properties("acme-pa.com");
  resourceOwner->add_owned_properties("acme-pb.co.uk");

  resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("bbco");
  resourceOwner->add_owned_resources()->set_domain("bbco-ra.com");
  resourceOwner->add_owned_resources()->set_domain("bbco-rb.co.ch");
  resourceOwner->add_owned_properties("bbco-pa.com");
  resourceOwner->add_owned_properties("bbco-pb.co.uk");

  allowList.UseMaskedDomainList(mdl);

  const MatchTest& p = GetParam();
  EXPECT_EQ(p.matches,
            allowList.Matches(GURL(base::StrCat({"https://", p.req})),
                              GURL(base::StrCat({"https://", p.top}))));
}

const std::vector<MatchTest> kMatchTests = {
    // First-party requests should never be proxied.
    MatchTest{"1PRsrcHost", "acme-ra.com", "acme-ra.com", false},
    MatchTest{"1PPropHost", "bbco-pb.co.uk", "bbco-pb.co.uk", false},
    MatchTest{"1POtherHost", "somehost.com", "somehost.com", false},

    // "First-party" is defined as schemefully same-site.
    MatchTest{"1PSameSiteOther1", "www.somehost.com", "somehost.com", false},
    MatchTest{"1PSameSiteOther2", "somehost.com", "www.somehost.com", false},
    MatchTest{"1PSameSiteRsrc1", "www.acme-ra.com", "acme-ra.com", false},
    MatchTest{"1PSameSiteRsrc2", "acme-ra.com", "www.acme-ra.com", false},
    MatchTest{"1PSameSiteRsrcSub1", "sub.sub.acme-ra.com", "acme-ra.com",
              false},
    MatchTest{"1PSameSiteRsrcSub2", "acme-ra.com", "sub.sub.acme-ra.com",
              false},
    MatchTest{"1PSameSiteProp1", "www.bbco-pb.co.uk", "bbco-pb.co.uk", false},
    MatchTest{"1PSameSiteProp2", "bbco-pb.co.uk", "www.bbco-pb.co.uk", false},

    // Third-party requests for hosts not appearing in the MDL should never be
    // proxied, regardless of the top-level.
    MatchTest{"3POtherReqInOther", "somehost.com", "otherhost.com", false},
    MatchTest{"3POtherReqInRsrc", "somehost.com", "acme-rb.co.uk", false},
    MatchTest{"3POtherReqInProp", "somehost.com", "bbco-pb.co.uk", false},

    // Third-party requests for resources (including subdomains) in the MDL
    // should be proxied (with exceptions below).
    MatchTest{"3PRsrcInOther", "acme-ra.com", "somehost.com", true},
    MatchTest{"3PRsrcInOtherRsrc", "acme-ra.com", "bbco-rb.co.ch", true},
    MatchTest{"3PRsrcInOtherProp", "acme-ra.com", "bbco-pa.com", true},
    MatchTest{"3PSubRsrc", "sub.acme-ra.com", "somehost.com", true},
    MatchTest{"3PSub2Rsrc", "sub.sub.acme-ra.com", "somehost.com", true},

    // Third-party requests for properties in the MDL should not be proxied.
    MatchTest{"3PPropInOther", "acme-pa.com", "somehost.com", false},
    MatchTest{"3PPropInOtherRsrc", "acme-pa.com", "bbco-rb.co.ch", false},
    MatchTest{"3PPropInOtherProp", "acme-pa.com", "bbco-pa.com", false},
    MatchTest{"3PPropInSameRsrc", "acme-pa.com", "acme-rb.co.uk", false},
    MatchTest{"3PPropInSameProp", "acme-pa.com", "acme-pb.co.uk", false},

    // As an exception, third-party requests for resources (including
    // subdomains) in the MDL should be not be proxied when the top-level site
    // is a property with the same owner as the resource.
    MatchTest{"3PRsrcInPropSameOwner", "acme-ra.com", "acme-pa.com", false},
    MatchTest{"3PRsrcInRsrcSameOwner", "acme-ra.com", "acme-rb.co.uk", false},
    MatchTest{"3PRsrcInSubRsrcSameOwner", "acme-ra.com", "sub.acme-rb.co.uk",
              false},
    MatchTest{"3PSubRsrcInSubRsrcSameOwner", "sub.acme-ra.com",
              "sub.acme-rb.co.uk", false},
    MatchTest{"3PSubSameOwner", "sub.acme-ra.com", "acme-pa.com", false},
    MatchTest{"3PSubSubSameOwner", "sub.sub.acme-ra.com", "acme-pa.com", false},
};

INSTANTIATE_TEST_SUITE_P(All,
                         NetworkServiceProxyAllowListMatchTest,
                         testing::ValuesIn(kMatchTests),
                         [](const testing::TestParamInfo<MatchTest>& info) {
                           return info.param.name;
                         });

}  // namespace network

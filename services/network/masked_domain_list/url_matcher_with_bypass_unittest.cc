// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/url_matcher_with_bypass.h"

#include "base/strings/strcat.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
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

class UrlMatcherWithBypassTest : public ::testing::Test {};

TEST_F(UrlMatcherWithBypassTest, PartitionMapKey) {
  auto PartitionMapKey = &UrlMatcherWithBypass::PartitionMapKey;
  EXPECT_EQ(PartitionMapKey("com"), "com");
  EXPECT_EQ(PartitionMapKey("foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("www.tiny.sub.foo.com"), "foo.com");
  EXPECT_EQ(PartitionMapKey("foo.co.uk"), "co.uk");
}

class UrlMatcherWithBypassMatchTest : public testing::TestWithParam<MatchTest> {
};

TEST_P(UrlMatcherWithBypassMatchTest, Match) {
  UrlMatcherWithBypass matcher;
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

  for (auto owner : mdl.resource_owners()) {
    for (auto resource : owner.owned_resources()) {
      matcher.AddMaskedDomainListRules(resource.domain(), owner);
    }
  }

  const MatchTest& p = GetParam();
  EXPECT_EQ(p.matches,
            matcher.Matches(GURL(base::StrCat({"https://", p.req})),
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
                         UrlMatcherWithBypassMatchTest,
                         testing::ValuesIn(kMatchTests),
                         [](const testing::TestParamInfo<MatchTest>& info) {
                           return info.param.name;
                         });

}  // namespace network

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include <string_view>

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/features.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;

struct ExperimentGroupMatchTest {
  std::string name;
  std::string req;
  std::string top;
  // The proto has an int type but feature init needs a string representation.
  std::string experiment_group;
  bool matches;
};

const std::vector<ExperimentGroupMatchTest> kMatchTests = {
    ExperimentGroupMatchTest{
        "NoExperimentGroup_ExcludedFromResource",
        "experiment.com",
        "top.com",
        "0",
        false,
    },
    ExperimentGroupMatchTest{
        "NoExperimentGroup_DefaultResourceMatch",
        "example.com",
        "top.com",
        "0",
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup1_ExperimentResourceMatch",
        "experiment.com",
        "top.com",
        "1",
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup2_ExperimentResourceMatch",
        "experiment.com",
        "top.com",
        "2",
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup1_DefaultResourceMatch",
        "example.com",
        "top.com",
        "1",
        true,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup2_ExcludedFromDefaultResource",
        "example.com",
        "top.com",
        "2",
        false,
    },
    ExperimentGroupMatchTest{
        "ExperimentGroup3_ExcludedFromDefaultResource",
        "experiment.com",
        "top.com",
        "3",
        false,
    },
};

constexpr std::string_view kTestDomain = "example.com";

}  // namespace

class NetworkServiceProxyAllowListBaseTest : public testing::Test {};

class NetworkServiceProxyAllowListTest
    : public NetworkServiceProxyAllowListBaseTest {
 public:
  NetworkServiceProxyAllowListTest()
      : allow_list_no_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::kNone),
        allow_list_first_party_bypass_(
            network::mojom::IpProtectionProxyBypassPolicy::
                kFirstPartyToTopLevelFrame) {}

  void SetUp() override {
    MaskedDomainList mdl;
    auto* resource_owner = mdl.add_resource_owners();
    resource_owner->set_owner_name("foo");
    resource_owner->add_owned_properties("property.com");
    resource_owner->add_owned_resources()->set_domain(std::string(kTestDomain));
    allow_list_no_bypass_.UseMaskedDomainList(
        mdl, /*exclusion_list=*/std::vector<std::string>());
    allow_list_first_party_bypass_.UseMaskedDomainList(
        mdl, /*exclusion_list=*/std::vector<std::string>());
  }

 protected:
  NetworkServiceProxyAllowList allow_list_no_bypass_;
  NetworkServiceProxyAllowList allow_list_first_party_bypass_;
};

TEST_F(NetworkServiceProxyAllowListBaseTest, IsNotEnabledByDefault) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  EXPECT_FALSE(allow_list_no_bypass.IsEnabled());
  EXPECT_FALSE(allow_list_first_party_bypass.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListBaseTest, IsEnabledWhenManuallySet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);

  EXPECT_TRUE(allow_list.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListBaseTest, AllowListIsNotPopulatedByDefault) {
  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  EXPECT_FALSE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListBaseTest, AllowlistIsPopulatedWhenMDLUsed) {
  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl,
                                 /*exclusion_list=*/std::vector<std::string>());

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, ShouldMatchHttp) {
  const auto kHttpRequestUrl = GURL(base::StrCat({"http://", kTestDomain}));
  const auto kHttpCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("http://top.com")));

  EXPECT_TRUE(
      allow_list_no_bypass_.Matches(kHttpRequestUrl, kHttpCrossSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpRequestUrl,
                                                     kHttpCrossSiteNak));
}

TEST_F(NetworkServiceProxyAllowListTest, ShouldMatchThirdPartyToTopLevelFrame) {
  const auto kHttpsCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("https://top.com")));
  const auto kHttpsSameSiteNak = net::NetworkAnonymizationKey::CreateSameSite(
      net::SchemefulSite(GURL("https://top.com")));
  const auto kHttpsThirdPartyRequestUrl =
      GURL(base::StrCat({"https://", kTestDomain}));

  // Regardless of whether the NAK is cross-site, the request URL should be
  // considered third-party because the request URL doesn't match the top-level
  // site.
  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                            kHttpsCrossSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                                     kHttpsCrossSiteNak));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                            kHttpsSameSiteNak));
  EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsThirdPartyRequestUrl,
                                                     kHttpsSameSiteNak));
}

TEST_F(NetworkServiceProxyAllowListTest,
       MatchFirstPartyToTopLevelFrameDependsOnBypass) {
  const auto kHttpsFirstPartyRequestUrl =
      GURL(base::StrCat({"https://", kTestDomain}));
  const auto kHttpsSameSiteNak = net::NetworkAnonymizationKey::CreateSameSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));
  const auto kHttpsCrossSiteNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(kHttpsFirstPartyRequestUrl));

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsFirstPartyRequestUrl,
                                            kHttpsSameSiteNak));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsSameSiteNak));
  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsFirstPartyRequestUrl,
                                            kHttpsCrossSiteNak));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(
      kHttpsFirstPartyRequestUrl, kHttpsCrossSiteNak));
}

TEST_F(NetworkServiceProxyAllowListTest,
       MatchFirstPartyToTopLevelFrameIfEmptyNakDependsOnBypass) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kEmptyNak = net::NetworkAnonymizationKey();

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kEmptyNak));
  EXPECT_FALSE(
      allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kEmptyNak));
}

TEST_F(NetworkServiceProxyAllowListTest,
       ShouldNotMatchWithFencedFrameNakIfUrlDoesNotMatch) {
  const auto kHttpsOtherRequestUrl = GURL("https://other.com");
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_FALSE(
      allow_list_no_bypass_.Matches(kHttpsOtherRequestUrl, kNakWithNonce));
  EXPECT_FALSE(allow_list_first_party_bypass_.Matches(kHttpsOtherRequestUrl,
                                                      kNakWithNonce));
}

TEST_F(NetworkServiceProxyAllowListTest,
       ShouldMatchWithFencedFrameNakIfUrlMatches) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kNakWithNonce = net::NetworkAnonymizationKey::CreateFromParts(
      net::SchemefulSite(), /*is_cross_site=*/true,
      base::UnguessableToken::Create());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kNakWithNonce));
  EXPECT_TRUE(
      allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kNakWithNonce));
}

TEST_F(NetworkServiceProxyAllowListTest, CustomSchemeTopLevelSite) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kExtensionUrlNak =
      net::NetworkAnonymizationKey::CreateCrossSite(net::SchemefulSite(
          GURL("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/")));
  ASSERT_FALSE(kExtensionUrlNak.IsTransient());

  EXPECT_TRUE(
      allow_list_no_bypass_.Matches(kHttpsRequestUrl, kExtensionUrlNak));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(allow_list_first_party_bypass_.Matches(kHttpsRequestUrl,
                                                        kExtensionUrlNak));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(allow_list_first_party_bypass_.Matches(kHttpsRequestUrl,
                                                       kExtensionUrlNak));
  }
}

// Test whether third-party requests from pages with a data: URL top-level site
// (where the corresponding NAK is transient) should be proxied.
TEST_F(NetworkServiceProxyAllowListTest, DataUrlTopLevelSite) {
  const auto kHttpsRequestUrl = GURL(base::StrCat({"https://", kTestDomain}));
  const auto kDataUrlNak = net::NetworkAnonymizationKey::CreateCrossSite(
      net::SchemefulSite(GURL("data:text/html,<html></html>")));
  ASSERT_TRUE(kDataUrlNak.IsTransient());

  EXPECT_TRUE(allow_list_no_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "true"}});
    EXPECT_FALSE(
        allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyRestrictTopLevelSiteSchemes.name, "false"}});
    EXPECT_TRUE(
        allow_list_first_party_bypass_.Matches(kHttpsRequestUrl, kDataUrlNak));
  }
}

TEST_F(NetworkServiceProxyAllowListTest, AllowListWithoutBypassUsesLessMemory) {
  EXPECT_GT(allow_list_first_party_bypass_.EstimateMemoryUsage(),
            allow_list_no_bypass_.EstimateMemoryUsage());
}

class NetworkServiceProxyAllowListExperimentGroupMatchTest
    : public NetworkServiceProxyAllowListBaseTest,
      public testing::WithParamInterface<ExperimentGroupMatchTest> {};

TEST_P(NetworkServiceProxyAllowListExperimentGroupMatchTest, Match) {
  const ExperimentGroupMatchTest& p = GetParam();

  std::map<std::string, std::string> parameters;
  parameters[network::features::kMaskedDomainListExperimentGroup.name] =
      p.experiment_group;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      network::features::kMaskedDomainList, std::move(parameters));

  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("example");
  auto* resource = resourceOwner->add_owned_resources();
  resource->set_domain("example.com");
  resource->add_experiment_group_ids(1);

  resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("experiment");
  resource = resourceOwner->add_owned_resources();
  resource->set_domain("experiment.com");
  resource->set_exclude_default_group(true);
  resource->add_experiment_group_ids(1);
  resource->add_experiment_group_ids(2);

  allow_list_no_bypass.UseMaskedDomainList(
      mdl, /*exclusion_list=*/std::vector<std::string>());
  allow_list_first_party_bypass.UseMaskedDomainList(
      mdl, /*exclusion_list=*/std::vector<std::string>());

  GURL request_url(base::StrCat({"https://", p.req}));
  auto network_anonymization_key =
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL(base::StrCat({"https://", p.top}))));

  EXPECT_EQ(p.matches, allow_list_no_bypass.Matches(request_url,
                                                    network_anonymization_key));
  EXPECT_EQ(p.matches, allow_list_no_bypass.Matches(request_url,
                                                    network_anonymization_key));
}

TEST_F(NetworkServiceProxyAllowListBaseTest,
       ExclusionSetDomainsRemovedFromMDL) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kExclusionList);
  std::set<std::string> mdl_domains(
      {"com", "example.com", "subdomain.example.com",
       "sub.subdomain.example.com", "unrelated-example.com", "example.net",
       "subdomain.example.net", "example.com.example.net", "excluded-tld",
       "included-tld", "subdomain.excluded-tld", "subdomain.included-tld"});
  std::set<std::string> exclusion_set(
      {"example.com", "excluded-tld", "irrelevant-tld"});
  std::set<std::string> mdl_domains_after_exclusions(
      {"com", "unrelated-example.com", "example.net", "subdomain.example.net",
       "example.com.example.net", "included-tld", "subdomain.included-tld"});
  std::set<std::string> empty_exclusion_set({});

  EXPECT_TRUE(allow_list_no_bypass.ExcludeDomainsFromMDL(
                  mdl_domains, exclusion_set) == mdl_domains_after_exclusions);
  EXPECT_TRUE(allow_list_no_bypass.ExcludeDomainsFromMDL(
                  mdl_domains, empty_exclusion_set) == mdl_domains);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NetworkServiceProxyAllowListExperimentGroupMatchTest,
    testing::ValuesIn(kMatchTests),
    [](const testing::TestParamInfo<ExperimentGroupMatchTest>& info) {
      return info.param.name;
    });

}  // namespace network

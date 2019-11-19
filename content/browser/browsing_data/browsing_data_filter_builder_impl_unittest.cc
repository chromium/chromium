// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_deletion_info.h"
#include "services/network/cookie_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using CookieDeletionInfo = net::CookieDeletionInfo;

namespace content {

namespace {

const char kGoogleDomain[] = "google.com";
// sp.nom.br is an eTLD, so this is a regular valid registrable domain, just
// like google.com.
const char kLongETLDDomain[] = "website.sp.nom.br";
// This domain will also not be found in registries, and since it has only
// one component, it will not be recognized as a valid registrable domain.
const char kInternalHostname[] = "fileserver";
// This domain will not be found in registries. It will be assumed that
// it belongs to an unknown registry, and since it has two components,
// they will be treated as the second level domain and TLD. Most importantly,
// it will NOT be treated as a subdomain of "fileserver".
const char kUnknownRegistryDomain[] = "second-level-domain.fileserver";
// IP addresses are supported.
const char kIPAddress[] = "192.168.1.1";

struct TestCase {
  std::string url;
  bool should_match;
};

void RunTestCase(TestCase test_case,
                 const base::RepeatingCallback<bool(const GURL&)>& filter) {
  GURL url(test_case.url);
  EXPECT_TRUE(url.is_valid()) << test_case.url << " is not valid.";
  EXPECT_EQ(test_case.should_match, filter.Run(GURL(test_case.url)))
      << test_case.url;
}

void RunTestCase(TestCase test_case,
                 network::mojom::CookieDeletionFilterPtr deletion_filter) {
  // Test with regular cookie, http only, domain, and secure.
  CookieDeletionInfo delete_info =
      network::DeletionFilterToInfo(std::move(deletion_filter));
  std::string cookie_line = "A=2";
  GURL test_url(test_case.url);
  EXPECT_TRUE(test_url.is_valid()) << test_case.url;
  std::unique_ptr<net::CanonicalCookie> cookie =
      net::CanonicalCookie::Create(test_url, cookie_line, base::Time::Now(),
                                   base::nullopt /* server_time */);
  EXPECT_TRUE(cookie) << cookie_line << " from " << test_case.url
                      << " is not a valid cookie";
  if (cookie)
    EXPECT_EQ(test_case.should_match, delete_info.Matches(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2;domain=") + test_url.host();
  cookie =
      net::CanonicalCookie::Create(test_url, cookie_line, base::Time::Now(),
                                   base::nullopt /* server_time */);
  if (cookie)
    EXPECT_EQ(test_case.should_match, delete_info.Matches(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2; HttpOnly;") + test_url.host();
  cookie =
      net::CanonicalCookie::Create(test_url, cookie_line, base::Time::Now(),
                                   base::nullopt /* server_time */);
  if (cookie)
    EXPECT_EQ(test_case.should_match, delete_info.Matches(*cookie))
        << cookie->DebugString();

  cookie_line = std::string("A=2; HttpOnly; Secure;") + test_url.host();
  cookie =
      net::CanonicalCookie::Create(test_url, cookie_line, base::Time::Now(),
                                   base::nullopt /* server_time */);
  if (cookie)
    EXPECT_EQ(test_case.should_match, delete_info.Matches(*cookie))
        << cookie->DebugString();
}

void RunTestCase(
    TestCase test_case,
    const base::RepeatingCallback<bool(const std::string&)>& filter) {
  std::string channel_id_server_id = test_case.url;
  EXPECT_EQ(test_case.should_match, filter.Run(channel_id_server_id))
      << channel_id_server_id << " should "
      << (test_case.should_match ? "" : "NOT ") << "be matched by the filter.";
}

}  // namespace

TEST(BrowsingDataFilterBuilderImplTest, Noop) {
  // An no-op filter matches everything.
  base::RepeatingCallback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  TestCase test_cases[] = {
      {"https://www.google.com", true},
      {"https://www.chrome.com", true},
      {"http://www.google.com/foo/bar", true},
      {"https://website.sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainGURLWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // We match any URL on the specified domains.
      {"http://www.google.com/foo/bar", true},
      {"https://www.sub.google.com/foo/bar", true},
      {"https://sub.google.com", true},
      {"http://www.sub.google.com:8000/foo/bar", true},
      {"https://website.sp.nom.br", true},
      {"https://www.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:80", true},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver/foo/bar", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", true},
      {"http://www.second-level-domain.fileserver/index.html", true},

      // Different domains.
      {"https://www.youtube.com", false},
      {"https://www.google.net", false},
      {"http://192.168.1.2", false},

      // Check both a bare eTLD.
      {"https://sp.nom.br", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainGURLBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // We match any URL that are not on the specified domains.
      {"http://www.google.com/foo/bar", false},
      {"https://www.sub.google.com/foo/bar", false},
      {"https://sub.google.com", false},
      {"http://www.sub.google.com:8000/foo/bar", false},
      {"https://website.sp.nom.br", false},
      {"https://www.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:80", false},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver/foo/bar", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", false},
      {"http://www.second-level-domain.fileserver/index.html", false},

      // Different domains.
      {"https://www.youtube.com", true},
      {"https://www.google.net", true},
      {"http://192.168.1.2", true},

      // Check our bare eTLD.
      {"https://sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesCookiesWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));

  TestCase test_cases[] = {
      // Any cookie with the same registerable domain as the origins is matched.
      {"https://www.google.com", true},
      {"http://www.google.com", true},
      {"http://www.google.com:300", true},
      {"https://mail.google.com", true},
      {"http://mail.google.com", true},
      {"http://google.com", true},
      {"https://website.sp.nom.br", true},
      {"https://sub.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:10", true},

      // Different eTLDs.
      {"https://www.google.org", false},
      {"https://www.google.co.uk", false},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", false},
      {"https://sp.nom.br", false},

      // Different hosts in general.
      {"https://www.chrome.com", false},
      {"http://192.168.2.1", false},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", true},
      {"https://subdomain.second-level-domain.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, builder.BuildCookieDeletionFilter());
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesCookiesBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));

  TestCase test_cases[] = {
      // Any cookie that doesn't have the same registerable domain is matched.
      {"https://www.google.com", false},
      {"http://www.google.com", false},
      {"http://www.google.com:300", false},
      {"https://mail.google.com", false},
      {"http://mail.google.com", false},
      {"http://google.com", false},
      {"https://website.sp.nom.br", false},
      {"https://sub.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:10", false},

      // Different eTLDs.
      {"https://www.google.org", true},
      {"https://www.google.co.uk", true},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", true},
      {"https://sp.nom.br", true},

      // Different hosts in general.
      {"https://www.chrome.com", true},
      {"http://192.168.2.1", true},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", false},
      {"https://subdomain.second-level-domain.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, builder.BuildCookieDeletionFilter());
}

TEST(BrowsingDataFilterBuilderImplTest, NetworkServiceFilterWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  ASSERT_EQ(BrowsingDataFilterBuilderImpl::WHITELIST, builder.GetMode());
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  network::mojom::ClearDataFilterPtr filter =
      builder.BuildNetworkServiceFilter();

  EXPECT_EQ(network::mojom::ClearDataFilter_Type::DELETE_MATCHES, filter->type);
  EXPECT_THAT(filter->domains, testing::UnorderedElementsAre(
                                   kGoogleDomain, kLongETLDDomain, kIPAddress,
                                   kUnknownRegistryDomain, kInternalHostname));
  EXPECT_TRUE(filter->origins.empty());
}

TEST(BrowsingDataFilterBuilderImplTest, NetworkServiceFilterBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  ASSERT_EQ(BrowsingDataFilterBuilderImpl::BLACKLIST, builder.GetMode());
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  network::mojom::ClearDataFilterPtr filter =
      builder.BuildNetworkServiceFilter();

  EXPECT_EQ(network::mojom::ClearDataFilter_Type::KEEP_MATCHES, filter->type);
  EXPECT_THAT(filter->domains, testing::UnorderedElementsAre(
                                   kGoogleDomain, kLongETLDDomain, kIPAddress,
                                   kUnknownRegistryDomain, kInternalHostname));
  EXPECT_TRUE(filter->origins.empty());
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesPluginSitesWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const std::string&)> filter =
      builder.BuildPluginFilter();

  TestCase test_cases[] = {
      // Plugin sites can be domains, ...
      {"google.com", true},
      {"www.google.com", true},
      {"website.sp.nom.br", true},
      {"www.website.sp.nom.br", true},
      {"second-level-domain.fileserver", true},
      {"foo.bar.second-level-domain.fileserver", true},

      // ... IP addresses, or internal hostnames.
      {"192.168.1.1", true},
      {"fileserver", true},

      // Sites not in the whitelist are not matched.
      {"example.com", false},
      {"192.168.1.2", false},
      {"website.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesPluginSitesBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const std::string&)> filter =
      builder.BuildPluginFilter();

  TestCase test_cases[] = {
      // Plugin sites can be domains, ...
      {"google.com", false},
      {"www.google.com", false},
      {"website.sp.nom.br", false},
      {"www.website.sp.nom.br", false},
      {"second-level-domain.fileserver", false},
      {"foo.bar.second-level-domain.fileserver", false},

      // ... IP addresses, or internal hostnames.
      {"192.168.1.1", false},
      {"fileserver", false},

      // Sites not in the blacklist are matched.
      {"example.com", true},
      {"192.168.1.2", true},
      {"website.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, OriginWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  builder.AddOrigin(url::Origin::Create(GURL("https://www.google.com")));
  builder.AddOrigin(url::Origin::Create(GURL("http://www.example.com")));
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // Whitelist matches any URL on the specified origins.
      { "https://www.google.com", true },
      { "https://www.google.com/?q=test", true },
      { "http://www.example.com", true },
      { "http://www.example.com/index.html", true },
      { "http://www.example.com/foo/bar", true },

      // Subdomains are different origins.
      { "https://test.www.google.com", false },

      // Different scheme or port is a different origin.
      { "https://www.google.com:8000", false },
      { "https://www.example.com/index.html", false },

      // Different host is a different origin.
      { "https://www.youtube.com", false },
      { "https://www.chromium.org", false },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, OriginBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  builder.AddOrigin(url::Origin::Create(GURL("https://www.google.com")));
  builder.AddOrigin(url::Origin::Create(GURL("http://www.example.com")));
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // URLS on explicitly specified origins are not matched.
      { "https://www.google.com", false },
      { "https://www.google.com/?q=test", false },
      { "http://www.example.com", false },
      { "http://www.example.com/index.html", false },
      { "http://www.example.com/foo/bar", false },

      // Subdomains are different origins.
      { "https://test.www.google.com", true },

      // The same hosts but with different schemes and ports
      // are not blacklisted.
      { "https://www.google.com:8000", true },
      { "https://www.example.com/index.html", true },

      // Different hosts are not blacklisted.
      { "https://www.chrome.com", true },
      { "https://www.youtube.com", true },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, CombinedWhitelist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::WHITELIST);
  builder.AddOrigin(url::Origin::Create(GURL("https://google.com")));
  builder.AddRegisterableDomain("example.com");
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // Whitelist matches any URL on the specified origins.
      { "https://google.com/foo/bar", true },
      { "https://example.com/?q=test", true },

      // Since www.google.com was added as an origin, its subdomains are not
      // matched. However, example.com was added as a registrable domain,
      // so its subdomains are matched.
      { "https://www.google.com/foo/bar", false },
      { "https://www.example.com/?q=test", true },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, CombinedBlacklist) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::BLACKLIST);
  builder.AddOrigin(url::Origin::Create(GURL("https://google.com")));
  builder.AddRegisterableDomain("example.com");
  base::RepeatingCallback<bool(const GURL&)> filter =
      builder.BuildGeneralFilter();

  TestCase test_cases[] = {
      // URLS on explicitly specified origins are not matched.
      { "https://google.com/foo/bar", false },
      { "https://example.com/?q=test", false },

      // Since www.google.com was added as an origin, its subdomains are
      // not in the blacklist. However, example.com was added as a registrable
      // domain, so its subdomains are also blacklisted.
      { "https://www.google.com/foo/bar", true },
      { "https://www.example.com/?q=test", false },
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

}  // namespace content

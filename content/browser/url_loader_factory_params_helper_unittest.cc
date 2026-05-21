// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/site_for_cookies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

class URLLoaderFactoryParamsHelperTest : public testing::Test {
 public:
  URLLoaderFactoryParamsHelperTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kUpdateIsMainFrameOriginRecentlyAccessed,
        {{net::features::kRecentlyAccessedOriginCacheSize.name, "4"}});
  }

 protected:
  net::IsolationInfo MakeIsolationInfo(const url::Origin& origin) {
    return net::IsolationInfo::Create(
        net::IsolationInfo::RequestType::kMainFrame, origin, origin,
        net::SiteForCookies::FromOrigin(origin));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(URLLoaderFactoryParamsHelperTest, IsMainFrameOriginRecentlyAccessed) {
  const GURL url1("https://a.com/a");
  const GURL url2("https://b.org/b");
  const GURL url3("https://c.com/c");
  const GURL url4("https://d.com/d");
  const url::Origin origin1 = url::Origin::Create(url1);
  const url::Origin origin2 = url::Origin::Create(url2);
  const url::Origin origin3 = url::Origin::Create(url3);
  const url::Origin origin4 = url::Origin::Create(url4);

  // No navigations yet.
  EXPECT_FALSE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin1)));

  // One navigation, origin matches.
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin1);
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin1)));

  // One navigation, origin does not match.
  EXPECT_FALSE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin2)));

  // Multiple navigations.
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin2);
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin3);
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin1)));
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin2)));
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin3)));

  // Origin not in navigations.
  EXPECT_FALSE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(url::Origin::Create(GURL("https://notfound.com")))));

  // Add another navigation.
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin4);
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin4)));
}

TEST_F(URLLoaderFactoryParamsHelperTest, LruCacheEviction) {
  const GURL url1("https://a1.com/1");
  const GURL url2("https://a2.com/2");
  const GURL url3("https://a3.com/3");
  const GURL url4("https://a4.com/4");
  const GURL url5("https://a5.com/5");

  const url::Origin origin1 = url::Origin::Create(url1);
  const url::Origin origin2 = url::Origin::Create(url2);
  const url::Origin origin3 = url::Origin::Create(url3);
  const url::Origin origin4 = url::Origin::Create(url4);
  const url::Origin origin5 = url::Origin::Create(url5);

  // Add five entries to the cache.
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin1);
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin2);
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin3);
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin4);
  URLLoaderFactoryParamsHelper::OnMainFrameNavigation(origin5);

  // The least recently used entry (origin1) should be evicted.
  EXPECT_FALSE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin1)));
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin2)));
  EXPECT_TRUE(URLLoaderFactoryParamsHelper::IsMainFrameOriginRecentlyAccessed(
      MakeIsolationInfo(origin4)));
}

// Tests for `ShouldPreferFactorySiteForCookies()`.
namespace {

// Stand-in for the chrome-extension:// scheme. Using a non-http(s)
// standard scheme keeps `IsolationInfo::Create` happy when the SFC and
// `frame_origin` legitimately disagree (the case we care about).
constexpr char kPrivilegedScheme[] = "fake-priv";

// `ContentBrowserClient` whose `ShouldUseFirstPartyStorageKey` mimics
// the production extension predicate by matching `kPrivilegedScheme`.
class PrivilegedSchemeBrowserClient : public ContentBrowserClient {
 public:
  bool ShouldUseFirstPartyStorageKey(const url::Origin& origin) override {
    return origin.scheme() == kPrivilegedScheme;
  }
};

class ScopedBrowserClientOverride {
 public:
  explicit ScopedBrowserClientOverride(ContentBrowserClient* new_client)
      : previous_(SetBrowserClientForTesting(new_client)) {}
  ~ScopedBrowserClientOverride() { SetBrowserClientForTesting(previous_); }

 private:
  raw_ptr<ContentBrowserClient> previous_;
};

net::IsolationInfo MakeIsolationInfoWithSiteForCookies(
    const url::Origin& top_frame_origin,
    const url::Origin& frame_origin,
    const net::SiteForCookies& site_for_cookies) {
  return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                    top_frame_origin, frame_origin,
                                    site_for_cookies);
}

}  // namespace

class URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest
    : public testing::Test {
 public:
  URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest() {
    url::AddStandardScheme(kPrivilegedScheme, url::SCHEME_WITH_HOST);
  }

 protected:
  url::ScopedSchemeRegistryForTests scoped_registry_;
  PrivilegedSchemeBrowserClient client_;
  ScopedBrowserClientOverride client_override_{&client_};
};

// Descendant frame inside a subtree whose effective top frame for
// storage partitioning differs from the actual top frame: factory
// `origin` is a regular web origin, but the IsolationInfo's
// `site_for_cookies` was set to the privileged origin by
// `ComputeIsolationInfoInternal()`. The flag must fire based on the
// SiteForCookies, not the factory origin, otherwise the network service
// drops back to the renderer's null `site_for_cookies` for these
// descendants and SameSite=Lax cookies break.
TEST_F(URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest,
       DescendantInEffectiveTopFrameSubtree) {
  const url::Origin extension_like_origin = url::Origin::Create(
      GURL(std::string(kPrivilegedScheme) + "://privileged-host/"));
  const url::Origin frame_origin =
      url::Origin::Create(GURL("https://viewer.example/"));
  net::IsolationInfo isolation_info = MakeIsolationInfoWithSiteForCookies(
      /*top_frame_origin=*/extension_like_origin,
      /*frame_origin=*/frame_origin,
      net::SiteForCookies::FromOrigin(extension_like_origin));

  EXPECT_TRUE(URLLoaderFactoryParamsHelper::ShouldPreferFactorySiteForCookies(
      /*has_effective_top_frame_for_storage_partitioning=*/true,
      isolation_info));
}

// Sanity: the privileged frame's own factory still sets the flag.
TEST_F(URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest,
       PrivilegedFrameItself) {
  const url::Origin extension_like_origin = url::Origin::Create(
      GURL(std::string(kPrivilegedScheme) + "://privileged-host/"));
  net::IsolationInfo isolation_info = MakeIsolationInfoWithSiteForCookies(
      /*top_frame_origin=*/extension_like_origin,
      /*frame_origin=*/extension_like_origin,
      net::SiteForCookies::FromOrigin(extension_like_origin));

  EXPECT_TRUE(URLLoaderFactoryParamsHelper::ShouldPreferFactorySiteForCookies(
      /*has_effective_top_frame_for_storage_partitioning=*/true,
      isolation_info));
}

// Negative: ordinary first-party fetch with a regular SiteForCookies.
TEST_F(URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest,
       NonPrivilegedContext) {
  const url::Origin origin = url::Origin::Create(GURL("https://example.test/"));
  net::IsolationInfo isolation_info = MakeIsolationInfoWithSiteForCookies(
      /*top_frame_origin=*/origin,
      /*frame_origin=*/origin, net::SiteForCookies::FromOrigin(origin));

  EXPECT_FALSE(URLLoaderFactoryParamsHelper::ShouldPreferFactorySiteForCookies(
      /*has_effective_top_frame_for_storage_partitioning=*/true,
      isolation_info));
}

// Negative: null SiteForCookies (cross-site embedder, no override) - the
// flag is meaningless because the network-service consumer ignores it
// when the factory's site_for_cookies is null.
TEST_F(URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest,
       NullSiteForCookies) {
  const url::Origin top_frame_origin =
      url::Origin::Create(GURL("https://example.test/"));
  const url::Origin frame_origin =
      url::Origin::Create(GURL("https://viewer.example/"));
  net::IsolationInfo isolation_info = MakeIsolationInfoWithSiteForCookies(
      /*top_frame_origin=*/top_frame_origin,
      /*frame_origin=*/frame_origin, net::SiteForCookies());

  EXPECT_FALSE(URLLoaderFactoryParamsHelper::ShouldPreferFactorySiteForCookies(
      /*has_effective_top_frame_for_storage_partitioning=*/true,
      isolation_info));
}

// Negative: even with a privileged `site_for_cookies`, the predicate is
// false when the frame's effective top frame for storage partitioning
// matches the actual top frame, because the renderer-computed SFC is
// already correct there.
TEST_F(URLLoaderFactoryParamsHelperPreferFactorySiteForCookiesTest,
       NoEffectiveTopFrameWithPrivilegedSiteForCookies) {
  const url::Origin extension_like_origin = url::Origin::Create(
      GURL(std::string(kPrivilegedScheme) + "://privileged-host/"));
  net::IsolationInfo isolation_info = MakeIsolationInfoWithSiteForCookies(
      /*top_frame_origin=*/extension_like_origin,
      /*frame_origin=*/extension_like_origin,
      net::SiteForCookies::FromOrigin(extension_like_origin));

  EXPECT_FALSE(URLLoaderFactoryParamsHelper::ShouldPreferFactorySiteForCookies(
      /*has_effective_top_frame_for_storage_partitioning=*/false,
      isolation_info));
}

}  // namespace content

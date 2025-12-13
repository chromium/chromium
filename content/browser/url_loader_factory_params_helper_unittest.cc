// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_params_helper.h"

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

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

}  // namespace content

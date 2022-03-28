// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_service.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_features.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/fake_service_worker_context.h"
#include "content/public/test/test_renderer_host.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchServiceTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());

    scoped_feature_list_.InitAndEnableFeature(
        content::features::kPrefetchUseContentRefactor);

    PrefetchService::SetHostNonUniqueFilterForTesting(
        [](base::StringPiece) { return false; });
    PrefetchService::SetServiceWorkerContextForTesting(
        &service_worker_context_);

    prefetch_service_ = PrefetchService::CreateIfPossible(browser_context());
  }

  void TearDown() override {
    prefetch_service_.reset();
    PrefetchService::SetHostNonUniqueFilterForTesting(nullptr);
    PrefetchService::SetServiceWorkerContextForTesting(nullptr);
    RenderViewHostTestHarness::TearDown();
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));

    EXPECT_TRUE(cookie.get());

    bool result = false;
    base::RunLoop run_loop;

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        *cookie.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));
    run_loop.Run();
    return result;
  }

 protected:
  FakeServiceWorkerContext service_worker_context_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PrefetchService> prefetch_service_;
};

TEST_F(PrefetchServiceTest, CreateServiceWhenFeatureEnabled) {
  // Enable feature, which means that we should be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_TRUE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, DontCreateServiceWhenFeatureDisabled) {
  // Disable feature, which means that we shouldn't be able to create a
  // PrefetchService instance.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      content::features::kPrefetchUseContentRefactor);

  EXPECT_FALSE(PrefetchService::CreateIfPossible(browser_context()));
}

TEST_F(PrefetchServiceTest, PrefetchPassEligibilityTest) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

TEST_F(PrefetchServiceTest, NotEligibleHostnameNonUnique) {
  PrefetchService::SetHostNonUniqueFilterForTesting(
      [](base::StringPiece) { return true; });

  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique);
}

TEST_F(PrefetchServiceTest, NotEligibleNonHttps) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("http://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps);
}

TEST_F(PrefetchServiceTest, EligibleNonHttpsNonProxiedPotentiallyTrustworthy) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("http://localhost"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/false));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

TEST_F(PrefetchServiceTest, NotEligibleServiceWorkerRegistered) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://example.com"))));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker);
}

TEST_F(PrefetchServiceTest, EligibleServiceWorkerNotRegistered) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  service_worker_context_.AddRegistrationToRegisteredStorageKeys(
      blink::StorageKey(url::Origin::Create(GURL("https://other.com"))));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

TEST_F(PrefetchServiceTest, NotEligibleUserHasCookies) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  ASSERT_TRUE(SetCookie(GURL("https://example.com"), "testing"));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotEligibleUserHasCookies);
}

TEST_F(PrefetchServiceTest, EligibleUserHasCookiesForDifferentUrl) {
  // TODO Double check these are in the "excluded" cookies list
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(), GURL("https://example.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  ASSERT_TRUE(SetCookie(GURL("https://other.com"), "testing"));

  prefetch_service_->PrefetchUrl(prefetch_container.GetWeakPtr());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

// TODO(https://crbug.com/1299059): Add test for incognito mode.

}  // namespace
}  // namespace content

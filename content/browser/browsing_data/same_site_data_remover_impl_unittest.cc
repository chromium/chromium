// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/browser/browsing_data/browsing_data_test_utils.h"
#include "content/browser/browsing_data/same_site_data_remover_impl.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_storage_partition.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace content {

struct StoragePartitionSameSiteRemovalData {
  uint32_t removal_mask = 0;
  uint32_t quota_storage_removal_mask = 0;
  std::unique_ptr<BrowsingDataFilterBuilder> filter_builder;
  StoragePartition::StorageKeyPolicyMatcherFunction storage_key_policy_matcher;
};

class SameSiteRemoverTestStoragePartition : public TestStoragePartition {
 public:
  SameSiteRemoverTestStoragePartition() {}

  SameSiteRemoverTestStoragePartition(
      const SameSiteRemoverTestStoragePartition&) = delete;
  SameSiteRemoverTestStoragePartition& operator=(
      const SameSiteRemoverTestStoragePartition&) = delete;

  ~SameSiteRemoverTestStoragePartition() override {}

  void ClearData(uint32_t removal_mask,
                 uint32_t quota_storage_removal_mask,
                 BrowsingDataFilterBuilder* filter_builder,
                 StorageKeyPolicyMatcherFunction storage_key_policy_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override {
    DCHECK(filter_builder);
    storage_partition_removal_data_.removal_mask = removal_mask;
    storage_partition_removal_data_.quota_storage_removal_mask =
        quota_storage_removal_mask;
    storage_partition_removal_data_.filter_builder = filter_builder->Copy();
    storage_partition_removal_data_.storage_key_policy_matcher =
        std::move(storage_key_policy_matcher);
    std::move(callback).Run();
  }

  const StoragePartitionSameSiteRemovalData& GetStoragePartitionRemovalData() {
    return storage_partition_removal_data_;
  }

 private:
  StoragePartitionSameSiteRemovalData storage_partition_removal_data_;
};

class SameSiteDataRemoverImplTest : public testing::Test {
 public:
  SameSiteDataRemoverImplTest()
      : browser_context_(std::make_unique<TestBrowserContext>()),
        same_site_remover_(
            std::make_unique<SameSiteDataRemoverImpl>(browser_context_.get())) {
  }

  SameSiteDataRemoverImplTest(const SameSiteDataRemoverImplTest&) = delete;
  SameSiteDataRemoverImplTest& operator=(const SameSiteDataRemoverImplTest&) =
      delete;

  void TearDown() override { browser_context_.reset(); }

  SameSiteDataRemoverImpl* GetSameSiteDataRemoverImpl() {
    return same_site_remover_.get();
  }

  BrowserContext* GetBrowserContext() { return browser_context_.get(); }

  void DeleteSameSiteNoneCookies() {
    base::RunLoop run_loop;
    GetSameSiteDataRemoverImpl()->DeleteSameSiteNoneCookies(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ClearStoragePartitionData() {
    base::RunLoop run_loop;
    GetSameSiteDataRemoverImpl()->ClearStoragePartitionData(
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void ClearStoragePartitionForOrigins(
      const std::set<std::string>& serialized_origins) {
    std::set<url::Origin> origins;
    for (const std::string& serialized_origin : serialized_origins) {
      origins.insert(url::Origin::Create(GURL(serialized_origin)));
    }
    base::RunLoop run_loop;
    GetSameSiteDataRemoverImpl()->ClearStoragePartitionForOrigins(
        run_loop.QuitClosure(), std::move(origins));
    run_loop.Run();
  }

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<SameSiteDataRemoverImpl> same_site_remover_;
};

TEST_F(SameSiteDataRemoverImplTest, TestRemoveSameSiteNoneCookies) {
  BrowserContext* browser_context = GetBrowserContext();

  CreateCookieForTest(
      "TestCookie1", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, browser_context);
  CreateCookieForTest(
      "TestCookie2", "www.gmail.google.com",
      net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, browser_context);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com", "www.gmail.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  EXPECT_THAT(cookies, IsEmpty());
}

TEST_F(SameSiteDataRemoverImplTest, TestRemoveOnlySameSiteNoneCookies) {
  BrowserContext* browser_context = GetBrowserContext();
  CreateCookieForTest(
      "TestCookie1", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, browser_context);
  // The second cookie has SameSite value STRICT_MODE instead of NO_RESTRICTION.
  CreateCookieForTest("TestCookie2", "www.gmail.google.com",
                      net::CookieSameSite::STRICT_MODE,
                      net::CookieOptions::SameSiteCookieContext(
                          net::CookieOptions::SameSiteCookieContext::
                              ContextType::SAME_SITE_STRICT),
                      true /* is_cookie_secure */, browser_context);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestRemoveSameDomainCookies) {
  BrowserContext* browser_context = GetBrowserContext();
  CreateCookieForTest(
      "TestCookie1", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, browser_context);
  // The second cookie has the same domain as the first cookie, but also has
  // SameSite value STRICT_MODE instead of NO_RESTRICTION.
  CreateCookieForTest("TestCookie2", "www.google.com",
                      net::CookieSameSite::STRICT_MODE,
                      net::CookieOptions::SameSiteCookieContext(
                          net::CookieOptions::SameSiteCookieContext::
                              ContextType::SAME_SITE_STRICT),
                      false /* is_cookie_secure */, browser_context);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("www.google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestKeepSameSiteCookies) {
  BrowserContext* browser_context = GetBrowserContext();
  CreateCookieForTest("TestCookie1", "www.google.com",
                      net::CookieSameSite::LAX_MODE,
                      net::CookieOptions::SameSiteCookieContext(
                          net::CookieOptions::SameSiteCookieContext::
                              ContextType::SAME_SITE_LAX),
                      false /* is_cookie_secure */, browser_context);
  CreateCookieForTest("TestCookie2", "www.gmail.google.com",
                      net::CookieSameSite::STRICT_MODE,
                      net::CookieOptions::SameSiteCookieContext(
                          net::CookieOptions::SameSiteCookieContext::
                              ContextType::SAME_SITE_STRICT),
                      false /* is_cookie_secure */, browser_context);

  DeleteSameSiteNoneCookies();

  ASSERT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              IsEmpty());

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  EXPECT_THAT(2u, cookies.size());
}

TEST_F(SameSiteDataRemoverImplTest, TestCookieRemovalUnaffectedByParameters) {
  BrowserContext* browser_context = GetBrowserContext();
  network::mojom::CookieManager* cookie_manager =
      GetCookieManager(browser_context);

  base::RunLoop run_loop1;
  net::CookieOptions options;
  options.set_include_httponly();
  bool result_out = false;
  auto cookie1 = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie1", "20", "google.com", "/", base::Time::Now(), base::Time(),
      base::Time(), base::Time(), true, true,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_HIGH,
      /*same_party=*/true);
  cookie_manager->SetCanonicalCookie(
      *cookie1, net::cookie_util::SimulatedCookieSource(*cookie1, "https"),
      options, base::BindLambdaForTesting([&](net::CookieAccessResult result) {
        result_out = result.status.IsInclude();
        run_loop1.Quit();
      }));
  run_loop1.Run();
  EXPECT_TRUE(result_out);

  base::RunLoop run_loop2;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::
              SAME_SITE_LAX));
  result_out = false;
  auto cookie2 = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "TestCookie2", "10", "gmail.google.com", "/", base::Time(),
      base::Time::Now() + base::Days(100), base::Time(), base::Time(), false,
      true, net::CookieSameSite::LAX_MODE, net::COOKIE_PRIORITY_HIGH, false);
  cookie_manager->SetCanonicalCookie(
      *cookie2, net::cookie_util::SimulatedCookieSource(*cookie2, "https"),
      options, base::BindLambdaForTesting([&](net::CookieAccessResult result) {
        result_out = result.status.IsInclude();
        run_loop2.Quit();
      }));
  run_loop2.Run();
  EXPECT_TRUE(result_out);

  DeleteSameSiteNoneCookies();

  EXPECT_THAT(GetSameSiteDataRemoverImpl()->GetDeletedDomainsForTesting(),
              UnorderedElementsAre("google.com"));

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "TestCookie2");
}

TEST_F(SameSiteDataRemoverImplTest, TestStoragePartitionDataRemoval) {
  BrowserContext* browser_context = GetBrowserContext();
  network::mojom::CookieManager* cookie_manager =
      GetCookieManager(browser_context);
  SameSiteRemoverTestStoragePartition storage_partition;
  storage_partition.set_cookie_manager_for_browser_process(cookie_manager);
  GetSameSiteDataRemoverImpl()->OverrideStoragePartitionForTesting(
      &storage_partition);

  CreateCookieForTest(
      "TestCookie1", ".google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      true /* is_cookie_secure */, browser_context);
  DeleteSameSiteNoneCookies();

  ClearStoragePartitionData();
  const StoragePartitionSameSiteRemovalData& removal_data =
      storage_partition.GetStoragePartitionRemovalData();

  const uint32_t expected_removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_ALL &
      ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  EXPECT_EQ(removal_data.removal_mask, expected_removal_mask);

  const uint32_t expected_quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  EXPECT_EQ(removal_data.quota_storage_removal_mask,
            expected_quota_storage_mask);

  ASSERT_TRUE(removal_data.filter_builder);
  EXPECT_TRUE(removal_data.storage_key_policy_matcher.is_null());

  StoragePartition::StorageKeyMatcherFunction storage_key_matcher =
      removal_data.filter_builder->BuildStorageKeyFilter();

  EXPECT_TRUE(
      storage_key_matcher.Run(blink::StorageKey::CreateFromStringForTesting(
          "http://www.google.com/test")));
  EXPECT_TRUE(storage_key_matcher.Run(
      blink::StorageKey::CreateFromStringForTesting("http://google.com")));
  EXPECT_FALSE(storage_key_matcher.Run(
      blink::StorageKey::CreateFromStringForTesting("http://youtube.com")));
}

TEST_F(SameSiteDataRemoverImplTest, TestClearStoragePartitionsForOrigins) {
  BrowserContext* browser_context = GetBrowserContext();
  network::mojom::CookieManager* cookie_manager =
      GetCookieManager(browser_context);
  SameSiteRemoverTestStoragePartition storage_partition;
  storage_partition.set_cookie_manager_for_browser_process(cookie_manager);
  GetSameSiteDataRemoverImpl()->OverrideStoragePartitionForTesting(
      &storage_partition);

  std::set<std::string> clear_origins = {"https://a.com", "https://b.com"};
  ClearStoragePartitionForOrigins(clear_origins);
  const StoragePartitionSameSiteRemovalData& removal_data =
      storage_partition.GetStoragePartitionRemovalData();

  const uint32_t expected_removal_mask =
      content::StoragePartition::REMOVE_DATA_MASK_ALL &
      ~content::StoragePartition::REMOVE_DATA_MASK_COOKIES;
  EXPECT_EQ(removal_data.removal_mask, expected_removal_mask);

  const uint32_t expected_quota_storage_mask =
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL;
  EXPECT_EQ(removal_data.quota_storage_removal_mask,
            expected_quota_storage_mask);

  ASSERT_TRUE(removal_data.filter_builder);
  EXPECT_TRUE(removal_data.storage_key_policy_matcher.is_null());

  StoragePartition::StorageKeyMatcherFunction storage_key_matcher =
      removal_data.filter_builder->BuildStorageKeyFilter();

  EXPECT_TRUE(storage_key_matcher.Run(
      blink::StorageKey::CreateFromStringForTesting("https://a.com")));
  EXPECT_FALSE(storage_key_matcher.Run(
      blink::StorageKey::CreateFromStringForTesting("http://youtube.com")));
}

TEST_F(SameSiteDataRemoverImplTest, TestDoesNotDeletePartitionedCookies) {
  BrowserContext* browser_context = GetBrowserContext();
  network::mojom::CookieManager* cookie_manager =
      GetCookieManager(browser_context);
  SameSiteRemoverTestStoragePartition storage_partition;
  storage_partition.set_cookie_manager_for_browser_process(cookie_manager);
  GetSameSiteDataRemoverImpl()->OverrideStoragePartitionForTesting(
      &storage_partition);

  // Set an unpartitioned cookie.
  CreateCookieForTest(
      "unpartitioned", "www.google.com", net::CookieSameSite::NO_RESTRICTION,
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE),
      /*is_cookie_secure=*/true, browser_context);

  // Set a partitioned cookie.
  base::RunLoop run_loop;
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext(
          net::CookieOptions::SameSiteCookieContext::ContextType::CROSS_SITE));
  bool result_out;
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "__Host-partitioned", "1", "www.partitioned.com", "/", base::Time(),
      base::Time(), base::Time(), base::Time(), /*secure=*/true,
      /*httponly=*/false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_LOW,
      /*same_party=*/false,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevel.site")));
  cookie_manager->SetCanonicalCookie(
      *cookie, net::cookie_util::SimulatedCookieSource(*cookie, "https"),
      options, base::BindLambdaForTesting([&](net::CookieAccessResult result) {
        result_out = result.status.IsInclude();
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(result_out);

  DeleteSameSiteNoneCookies();
  ClearStoragePartitionData();
  const StoragePartitionSameSiteRemovalData& removal_data =
      storage_partition.GetStoragePartitionRemovalData();

  const std::vector<net::CanonicalCookie>& cookies =
      GetAllCookies(browser_context);
  ASSERT_EQ(1u, cookies.size());
  ASSERT_EQ(cookies[0].Name(), "__Host-partitioned");

  ASSERT_TRUE(removal_data.filter_builder);
  EXPECT_TRUE(removal_data.storage_key_policy_matcher.is_null());

  StoragePartition::StorageKeyMatcherFunction storage_key_matcher =
      removal_data.filter_builder->BuildStorageKeyFilter();

  EXPECT_TRUE(storage_key_matcher.Run(
      blink::StorageKey::CreateFromStringForTesting("https://www.google.com")));
  EXPECT_FALSE(
      storage_key_matcher.Run(blink::StorageKey::CreateFromStringForTesting(
          "https://www.partitioned.com")));
}

}  // namespace content

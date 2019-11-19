// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/browser/appcache/appcache_quota_client.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;
static const StorageType kPerm = StorageType::kPersistent;

// Base class for our test fixtures.
class AppCacheQuotaClientTest : public testing::Test {
 public:
  const url::Origin kOriginA;
  const url::Origin kOriginB;
  const url::Origin kOriginOther;

  AppCacheQuotaClientTest()
      : kOriginA(url::Origin::Create(GURL("http://host"))),
        kOriginB(url::Origin::Create(GURL("http://host:8000"))),
        kOriginOther(url::Origin::Create(GURL("http://other"))),
        usage_(0),
        delete_status_(blink::mojom::QuotaStatusCode::kUnknown),
        num_get_origin_usage_completions_(0),
        num_get_origins_completions_(0),
        num_delete_origins_completions_(0) {}

  int64_t GetOriginUsage(base::WeakPtr<storage::QuotaClient> client,
                         const url::Origin& origin,
                         StorageType type) {
    usage_ = -1;
    AsyncGetOriginUsage(std::move(client), origin, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::set<url::Origin>& GetOriginsForType(
      base::WeakPtr<storage::QuotaClient> client,
      StorageType type) {
    origins_.clear();
    AsyncGetOriginsForType(std::move(client), type);
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::set<url::Origin>& GetOriginsForHost(
      base::WeakPtr<storage::QuotaClient> client,
      StorageType type,
      const std::string& host) {
    origins_.clear();
    AsyncGetOriginsForHost(std::move(client), type, host);
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  blink::mojom::QuotaStatusCode DeleteOriginData(
      base::WeakPtr<storage::QuotaClient> client,
      StorageType type,
      const url::Origin& origin) {
    delete_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    AsyncDeleteOriginData(std::move(client), type, origin);
    base::RunLoop().RunUntilIdle();
    return delete_status_;
  }

  void AsyncGetOriginUsage(base::WeakPtr<storage::QuotaClient> client,
                           const url::Origin& origin,
                           StorageType type) {
    CHECK(client);
    client->GetOriginUsage(
        origin, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginUsageComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void AsyncGetOriginsForType(base::WeakPtr<storage::QuotaClient> client,
                              StorageType type) {
    CHECK(client);
    client->GetOriginsForType(
        type, base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginsComplete,
                             weak_factory_.GetWeakPtr()));
  }

  void AsyncGetOriginsForHost(base::WeakPtr<storage::QuotaClient> client,
                              StorageType type,
                              const std::string& host) {
    CHECK(client);
    client->GetOriginsForHost(
        type, host,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginsComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void AsyncDeleteOriginData(base::WeakPtr<storage::QuotaClient> client,
                             StorageType type,
                             const url::Origin& origin) {
    CHECK(client);
    client->DeleteOriginData(
        origin, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnDeleteOriginDataComplete,
                       weak_factory_.GetWeakPtr()));
  }

  void SetUsageMapEntry(const url::Origin& origin, int64_t usage) {
    mock_service_.storage()->usage_map_[origin] = usage;
  }

  base::WeakPtr<AppCacheQuotaClient> CreateClient() {
    // The bare operator new is used here because AppCacheQuotaClient deletes
    // itself when the QuotaManager goes out of scope.
    return (new AppCacheQuotaClient(mock_service_.AsWeakPtr()))->AsWeakPtr();
  }

  void Call_NotifyAppCacheReady(base::WeakPtr<AppCacheQuotaClient> client) {
    if (client)
      client->NotifyAppCacheReady();
  }

  void Call_NotifyAppCacheDestroyed(base::WeakPtr<AppCacheQuotaClient> client) {
    if (client)
      client->NotifyAppCacheDestroyed();
  }

  void Call_OnQuotaManagerDestroyed(base::WeakPtr<AppCacheQuotaClient> client) {
    if (client)
      client->OnQuotaManagerDestroyed();
  }

 protected:
  void OnGetOriginUsageComplete(int64_t usage) {
    ++num_get_origin_usage_completions_;
    usage_ = usage;
  }

  void OnGetOriginsComplete(const std::set<url::Origin>& origins) {
    ++num_get_origins_completions_;
    origins_ = origins;
  }

  void OnDeleteOriginDataComplete(blink::mojom::QuotaStatusCode status) {
    ++num_delete_origins_completions_;
    delete_status_ = status;
  }

  BrowserTaskEnvironment task_environment_;
  int64_t usage_;
  std::set<url::Origin> origins_;
  blink::mojom::QuotaStatusCode delete_status_;
  int num_get_origin_usage_completions_;
  int num_get_origins_completions_;
  int num_delete_origins_completions_;
  MockAppCacheService mock_service_;
  base::WeakPtrFactory<AppCacheQuotaClientTest> weak_factory_{this};
};

TEST_F(AppCacheQuotaClientTest, BasicCreateDestroy) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);
  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, QuotaManagerDestroyedInCallback) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);
  client->DeleteOriginData(kOriginA, kTemp,
                           base::BindOnce(
                               [](AppCacheQuotaClientTest* test,
                                  base::WeakPtr<AppCacheQuotaClient> client,
                                  blink::mojom::QuotaStatusCode) {
                                 test->Call_OnQuotaManagerDestroyed(client);
                               },
                               this, client));
  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, EmptyService) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));
  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kTemp, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kPerm, kOriginA.host()).empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(client, kPerm, kOriginA));

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, NoService) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);
  Call_NotifyAppCacheDestroyed(client);

  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));
  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kTemp, kOriginA.host()).empty());
  EXPECT_TRUE(GetOriginsForHost(client, kPerm, kOriginA.host()).empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteOriginData(client, kPerm, kOriginA));

  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginUsage) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  SetUsageMapEntry(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(client, kOriginA, kPerm));

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForHost) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::set<url::Origin> origins =
      GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  origins = GetOriginsForHost(client, kTemp, kOriginA.host());
  EXPECT_EQ(2ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  origins = GetOriginsForHost(client, kTemp, kOriginOther.host());
  EXPECT_EQ(1ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginOther) != origins.end());

  origins = GetOriginsForHost(client, kPerm, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForType) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  EXPECT_TRUE(GetOriginsForType(client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);

  std::set<url::Origin> origins = GetOriginsForType(client, kTemp);
  EXPECT_EQ(2ul, origins.size());
  EXPECT_TRUE(origins.find(kOriginA) != origins.end());
  EXPECT_TRUE(origins.find(kOriginB) != origins.end());

  EXPECT_TRUE(GetOriginsForType(client, kPerm).empty());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DeleteOriginData) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  // Perm deletions are short circuited in the Client and
  // should not reach the AppCacheServiceImpl.
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(client, kPerm, kOriginA));
  EXPECT_EQ(0, mock_service_.delete_called_count());

  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_service_.delete_called_count());

  mock_service_.set_mock_delete_appcaches_for_origin_result(
      net::ERR_ABORTED);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteOriginData(client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_service_.delete_called_count());

  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, PendingRequests) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);

  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Pending requests should get serviced when the appcache is ready.
  Call_NotifyAppCacheReady(client);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(3, num_delete_origins_completions_);

  // They should be serviced in order requested.
  EXPECT_EQ(10, usage_);
  EXPECT_EQ(1ul, origins_.size());
  EXPECT_TRUE(origins_.find(kOriginOther) != origins_.end());

  Call_NotifyAppCacheDestroyed(client);
  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyServiceWithPending) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts prior to being ready.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyAppCacheDestroyed(client);

  // All should have been aborted and called completion.
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(3, num_delete_origins_completions_);
  EXPECT_EQ(0, usage_);
  EXPECT_TRUE(origins_.empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnQuotaManagerDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyQuotaManagerWithPending) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some reqeusts prior to being ready.
  AsyncGetOriginUsage(client, kOriginA, kPerm);
  AsyncGetOriginUsage(client, kOriginB, kTemp);
  AsyncGetOriginsForType(client, kPerm);
  AsyncGetOriginsForType(client, kTemp);
  AsyncGetOriginsForHost(client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(client, kTemp, kOriginA);
  AsyncDeleteOriginData(client, kPerm, kOriginA);
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the quota manager.
  Call_OnQuotaManagerDestroyed(client);
  Call_NotifyAppCacheReady(client);

  // Callbacks should be deleted and not called.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  Call_NotifyAppCacheDestroyed(client);
}

TEST_F(AppCacheQuotaClientTest, DestroyWithDeleteInProgress) {
  base::WeakPtr<AppCacheQuotaClient> client = CreateClient();
  Call_NotifyAppCacheReady(client);

  // Start an async delete.
  AsyncDeleteOriginData(client, kTemp, kOriginB);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyAppCacheDestroyed(client);

  // Should have been aborted.
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  // A real completion callback from the service should
  // be dropped if it comes in after NotifyAppCacheDestroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnQuotaManagerDestroyed(client);
}

}  // namespace content

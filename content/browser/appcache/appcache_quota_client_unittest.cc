// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "content/browser/appcache/appcache_quota_client.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

using blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;

// Base class for our test fixtures.
class AppCacheQuotaClientTest : public testing::Test {
 public:
  const url::Origin kOriginA;
  const url::Origin kOriginB;
  const url::Origin kOriginOther;

  AppCacheQuotaClientTest()
      : kOriginA(url::Origin::Create(GURL("http://host"))),
        kOriginB(url::Origin::Create(GURL("http://host:8000"))),
        kOriginOther(url::Origin::Create(GURL("http://other"))) {}

  int64_t GetOriginUsage(storage::mojom::QuotaClient& client,
                         const url::Origin& origin,
                         StorageType type) {
    usage_ = -1;
    AsyncGetOriginUsage(client, origin, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::vector<url::Origin>& GetOriginsForType(
      storage::mojom::QuotaClient& client,
      StorageType type) {
    origins_.clear();
    AsyncGetOriginsForType(client, type);
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  const std::vector<url::Origin>& GetOriginsForHost(
      storage::mojom::QuotaClient& client,
      StorageType type,
      const std::string& host) {
    origins_.clear();
    AsyncGetOriginsForHost(client, type, host);
    base::RunLoop().RunUntilIdle();
    return origins_;
  }

  blink::mojom::QuotaStatusCode DeleteOriginData(
      storage::mojom::QuotaClient& client,
      StorageType type,
      const url::Origin& origin) {
    delete_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    AsyncDeleteOriginData(client, type, origin);
    base::RunLoop().RunUntilIdle();
    return delete_status_;
  }

  void AsyncGetOriginUsage(storage::mojom::QuotaClient& client,
                           const url::Origin& origin,
                           StorageType type) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetOriginUsage(
        origin, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginUsageComplete,
                       base::Unretained(this)));
  }

  void AsyncGetOriginsForType(storage::mojom::QuotaClient& client,
                              StorageType type) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetOriginsForType(
        type, base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginsComplete,
                             base::Unretained(this)));
  }

  void AsyncGetOriginsForHost(storage::mojom::QuotaClient& client,
                              StorageType type,
                              const std::string& host) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetOriginsForHost(
        type, host,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetOriginsComplete,
                       base::Unretained(this)));
  }

  void AsyncDeleteOriginData(storage::mojom::QuotaClient& client,
                             StorageType type,
                             const url::Origin& origin) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.DeleteOriginData(
        origin, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnDeleteOriginDataComplete,
                       base::Unretained(this)));
  }

  void SetUsageMapEntry(const url::Origin& origin, int64_t usage) {
    mock_service_.storage()->usage_map_[origin] = usage;
  }

  std::unique_ptr<AppCacheQuotaClient> CreateClient() {
    return std::make_unique<AppCacheQuotaClient>(mock_service_.AsWeakPtr());
  }

  void Call_NotifyStorageReady(AppCacheQuotaClient& client) {
    client.NotifyStorageReady();
  }

  void Call_NotifyServiceDestroyed(AppCacheQuotaClient& client) {
    client.NotifyServiceDestroyed();
  }

  void Call_OnMojoDisconnect(AppCacheQuotaClient& client) {
    client.OnMojoDisconnect();
  }

 protected:
  void OnGetOriginUsageComplete(int64_t usage) {
    ++num_get_origin_usage_completions_;
    usage_ = usage;
  }

  void OnGetOriginsComplete(const std::vector<url::Origin>& origins) {
    ++num_get_origins_completions_;
    origins_ = origins;
  }

  void OnDeleteOriginDataComplete(blink::mojom::QuotaStatusCode status) {
    ++num_delete_origins_completions_;
    delete_status_ = status;
  }

  BrowserTaskEnvironment task_environment_;
  int64_t usage_ = 0;
  std::vector<url::Origin> origins_;
  blink::mojom::QuotaStatusCode delete_status_ =
      blink::mojom::QuotaStatusCode::kUnknown;
  int num_get_origin_usage_completions_ = 0;
  int num_get_origins_completions_ = 0;
  int num_delete_origins_completions_ = 0;
  MockAppCacheService mock_service_;
};

TEST_F(AppCacheQuotaClientTest, BasicCreateDestroy) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);
  Call_OnMojoDisconnect(*client);
  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, QuotaManagerDestroyedInCallback) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);
  client->DeleteOriginData(
      kOriginA, kTemp,
      base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode) {
        Call_OnMojoDisconnect(*client);
      }));
  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, EmptyService) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(0, GetOriginUsage(*client, kOriginA, kTemp));
  EXPECT_TRUE(GetOriginsForType(*client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForHost(*client, kTemp, kOriginA.host()).empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(*client, kTemp, kOriginA));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, NoService) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);
  Call_NotifyServiceDestroyed(*client);

  EXPECT_EQ(0, GetOriginUsage(*client, kOriginA, kTemp));
  EXPECT_TRUE(GetOriginsForType(*client, kTemp).empty());
  EXPECT_TRUE(GetOriginsForHost(*client, kTemp, kOriginA.host()).empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteOriginData(*client, kTemp, kOriginA));

  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginUsage) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  SetUsageMapEntry(kOriginA, 1000);
  EXPECT_EQ(1000, GetOriginUsage(*client, kOriginA, kTemp));
  EXPECT_EQ(0, GetOriginUsage(*client, kOriginB, kTemp));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForHost) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(kOriginA.host(), kOriginB.host());
  EXPECT_NE(kOriginA.host(), kOriginOther.host());

  std::vector<url::Origin> origins =
      GetOriginsForHost(*client, kTemp, kOriginA.host());
  EXPECT_TRUE(origins.empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  origins = GetOriginsForHost(*client, kTemp, kOriginA.host());
  EXPECT_EQ(2ul, origins.size());
  EXPECT_THAT(origins, testing::Contains(kOriginA));
  EXPECT_THAT(origins, testing::Contains(kOriginB));

  origins = GetOriginsForHost(*client, kTemp, kOriginOther.host());
  EXPECT_EQ(1ul, origins.size());
  EXPECT_THAT(origins, testing::Contains(kOriginOther));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetOriginsForType) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_TRUE(GetOriginsForType(*client, kTemp).empty());

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);

  std::vector<url::Origin> origins = GetOriginsForType(*client, kTemp);
  EXPECT_EQ(2ul, origins.size());
  EXPECT_THAT(origins, testing::Contains(kOriginA));
  EXPECT_THAT(origins, testing::Contains(kOriginB));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DeleteOriginData) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteOriginData(*client, kTemp, kOriginA));
  EXPECT_EQ(1, mock_service_.delete_called_count());

  mock_service_.set_mock_delete_appcaches_for_origin_result(
      net::ERR_ABORTED);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteOriginData(*client, kTemp, kOriginA));
  EXPECT_EQ(2, mock_service_.delete_called_count());

  Call_OnMojoDisconnect(*client);
  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, PendingRequests) {
  auto client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some requests.
  AsyncGetOriginUsage(*client, kOriginA, kTemp);
  AsyncGetOriginUsage(*client, kOriginB, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForHost(*client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(*client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(*client, kTemp, kOriginA);
  AsyncDeleteOriginData(*client, kTemp, kOriginB);

  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Pending requests should get serviced when the appcache is ready.
  Call_NotifyStorageReady(*client);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(2, num_delete_origins_completions_);

  // They should be serviced in order requested.
  EXPECT_EQ(10, usage_);
  EXPECT_EQ(1ul, origins_.size());
  EXPECT_THAT(origins_, testing::Contains(kOriginOther));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyServiceWithPending) {
  auto client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some requests prior to being ready.
  AsyncGetOriginUsage(*client, kOriginA, kTemp);
  AsyncGetOriginUsage(*client, kOriginB, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForHost(*client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(*client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(*client, kTemp, kOriginA);
  AsyncDeleteOriginData(*client, kTemp, kOriginB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyServiceDestroyed(*client);

  // All should have been aborted and called completion.
  EXPECT_EQ(2, num_get_origin_usage_completions_);
  EXPECT_EQ(4, num_get_origins_completions_);
  EXPECT_EQ(2, num_delete_origins_completions_);
  EXPECT_EQ(0, usage_);
  EXPECT_TRUE(origins_.empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyQuotaManagerWithPending) {
  auto client = CreateClient();

  SetUsageMapEntry(kOriginA, 1000);
  SetUsageMapEntry(kOriginB, 10);
  SetUsageMapEntry(kOriginOther, 500);

  // Queue up some requests prior to being ready.
  AsyncGetOriginUsage(*client, kOriginA, kTemp);
  AsyncGetOriginUsage(*client, kOriginB, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForType(*client, kTemp);
  AsyncGetOriginsForHost(*client, kTemp, kOriginA.host());
  AsyncGetOriginsForHost(*client, kTemp, kOriginOther.host());
  AsyncDeleteOriginData(*client, kTemp, kOriginA);
  AsyncDeleteOriginData(*client, kTemp, kOriginB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the quota manager.
  Call_OnMojoDisconnect(*client);
  Call_NotifyStorageReady(*client);

  // Callbacks should be deleted and not called.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_origin_usage_completions_);
  EXPECT_EQ(0, num_get_origins_completions_);
  EXPECT_EQ(0, num_delete_origins_completions_);

  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyWithDeleteInProgress) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  // Start an async delete.
  AsyncDeleteOriginData(*client, kTemp, kOriginB);
  EXPECT_EQ(0, num_delete_origins_completions_);

  // Kill the service.
  Call_NotifyServiceDestroyed(*client);

  // Should have been aborted.
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  // A real completion callback from the service should
  // be dropped if it comes in after NotifyServiceDestroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_delete_origins_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnMojoDisconnect(*client);
}

}  // namespace content

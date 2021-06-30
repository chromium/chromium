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
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace content {

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

// Declared to shorten the line lengths.
static const StorageType kTemp = StorageType::kTemporary;

// Base class for our test fixtures.
class AppCacheQuotaClientTest : public testing::Test {
 public:
  const StorageKey kStorageKeyA;
  const StorageKey kStorageKeyB;
  const StorageKey kStorageKeyOther;

  AppCacheQuotaClientTest()
      : kStorageKeyA(StorageKey::CreateFromStringForTesting("http://host")),
        kStorageKeyB(
            StorageKey::CreateFromStringForTesting("http://host:8000")),
        kStorageKeyOther(
            StorageKey::CreateFromStringForTesting("http://other")) {}

  int64_t GetStorageKeyUsage(storage::mojom::QuotaClient& client,
                             const StorageKey& storage_key,
                             StorageType type) {
    usage_ = -1;
    AsyncGetStorageKeyUsage(client, storage_key, type);
    base::RunLoop().RunUntilIdle();
    return usage_;
  }

  const std::vector<StorageKey>& GetStorageKeysForType(
      storage::mojom::QuotaClient& client,
      StorageType type) {
    storage_keys_.clear();
    AsyncGetStorageKeysForType(client, type);
    base::RunLoop().RunUntilIdle();
    return storage_keys_;
  }

  const std::vector<StorageKey>& GetStorageKeysForHost(
      storage::mojom::QuotaClient& client,
      StorageType type,
      const std::string& host) {
    storage_keys_.clear();
    AsyncGetStorageKeysForHost(client, type, host);
    base::RunLoop().RunUntilIdle();
    return storage_keys_;
  }

  blink::mojom::QuotaStatusCode DeleteStorageKeyData(
      storage::mojom::QuotaClient& client,
      StorageType type,
      const StorageKey& storage_key) {
    delete_status_ = blink::mojom::QuotaStatusCode::kUnknown;
    AsyncDeleteStorageKeyData(client, type, storage_key);
    base::RunLoop().RunUntilIdle();
    return delete_status_;
  }

  void AsyncGetStorageKeyUsage(storage::mojom::QuotaClient& client,
                               const StorageKey& storage_key,
                               StorageType type) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetStorageKeyUsage(
        storage_key, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetStorageKeyUsageComplete,
                       base::Unretained(this)));
  }

  void AsyncGetStorageKeysForType(storage::mojom::QuotaClient& client,
                                  StorageType type) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetStorageKeysForType(
        type, base::BindOnce(&AppCacheQuotaClientTest::OnGetStorageKeysComplete,
                             base::Unretained(this)));
  }

  void AsyncGetStorageKeysForHost(storage::mojom::QuotaClient& client,
                                  StorageType type,
                                  const std::string& host) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.GetStorageKeysForHost(
        type, host,
        base::BindOnce(&AppCacheQuotaClientTest::OnGetStorageKeysComplete,
                       base::Unretained(this)));
  }

  void AsyncDeleteStorageKeyData(storage::mojom::QuotaClient& client,
                                 StorageType type,
                                 const StorageKey& storage_key) {
    // Unretained usage is safe because this test owns a TaskEnvironment. No
    // tasks will be executed after the test completes.
    client.DeleteStorageKeyData(
        storage_key, type,
        base::BindOnce(&AppCacheQuotaClientTest::OnDeleteStorageKeyDataComplete,
                       base::Unretained(this)));
  }

  void SetUsageMapEntry(const StorageKey& storage_key, int64_t usage) {
    mock_service_.storage()->usage_map_[storage_key.origin()] = usage;
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
  void OnGetStorageKeyUsageComplete(int64_t usage) {
    ++num_get_storage_key_usage_completions_;
    usage_ = usage;
  }

  void OnGetStorageKeysComplete(const std::vector<StorageKey>& storage_keys) {
    ++num_get_storage_keys_completions_;
    storage_keys_ = storage_keys;
  }

  void OnDeleteStorageKeyDataComplete(blink::mojom::QuotaStatusCode status) {
    ++num_delete_storage_keys_completions_;
    delete_status_ = status;
  }

  BrowserTaskEnvironment task_environment_;
  int64_t usage_ = 0;
  std::vector<StorageKey> storage_keys_;
  blink::mojom::QuotaStatusCode delete_status_ =
      blink::mojom::QuotaStatusCode::kUnknown;
  int num_get_storage_key_usage_completions_ = 0;
  int num_get_storage_keys_completions_ = 0;
  int num_delete_storage_keys_completions_ = 0;
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
  client->DeleteStorageKeyData(
      kStorageKeyA, kTemp,
      base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode) {
        Call_OnMojoDisconnect(*client);
      }));
  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, EmptyService) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(0, GetStorageKeyUsage(*client, kStorageKeyA, kTemp));
  EXPECT_TRUE(GetStorageKeysForType(*client, kTemp).empty());
  EXPECT_TRUE(
      GetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host())
          .empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteStorageKeyData(*client, kTemp, kStorageKeyA));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, NoService) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);
  Call_NotifyServiceDestroyed(*client);

  EXPECT_EQ(0, GetStorageKeyUsage(*client, kStorageKeyA, kTemp));
  EXPECT_TRUE(GetStorageKeysForType(*client, kTemp).empty());
  EXPECT_TRUE(
      GetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host())
          .empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteStorageKeyData(*client, kTemp, kStorageKeyA));

  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetStorageKeyUsage) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  SetUsageMapEntry(kStorageKeyA, 1000);
  EXPECT_EQ(1000, GetStorageKeyUsage(*client, kStorageKeyA, kTemp));
  EXPECT_EQ(0, GetStorageKeyUsage(*client, kStorageKeyB, kTemp));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetStorageKeysForHost) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(kStorageKeyA.origin().host(), kStorageKeyB.origin().host());
  EXPECT_NE(kStorageKeyA.origin().host(), kStorageKeyOther.origin().host());

  std::vector<StorageKey> storage_keys =
      GetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host());
  EXPECT_TRUE(storage_keys.empty());

  SetUsageMapEntry(kStorageKeyA, 1000);
  SetUsageMapEntry(kStorageKeyB, 10);
  SetUsageMapEntry(kStorageKeyOther, 500);

  storage_keys =
      GetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host());
  EXPECT_EQ(2ul, storage_keys.size());
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyB));

  storage_keys =
      GetStorageKeysForHost(*client, kTemp, kStorageKeyOther.origin().host());
  EXPECT_EQ(1ul, storage_keys.size());
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyOther));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, GetStorageKeysForType) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_TRUE(GetStorageKeysForType(*client, kTemp).empty());

  SetUsageMapEntry(kStorageKeyA, 1000);
  SetUsageMapEntry(kStorageKeyB, 10);

  std::vector<StorageKey> storage_keys = GetStorageKeysForType(*client, kTemp);
  EXPECT_EQ(2ul, storage_keys.size());
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyA));
  EXPECT_THAT(storage_keys, testing::Contains(kStorageKeyB));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DeleteStorageKeyData) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  EXPECT_EQ(blink::mojom::QuotaStatusCode::kOk,
            DeleteStorageKeyData(*client, kTemp, kStorageKeyA));
  EXPECT_EQ(1, mock_service_.delete_called_count());

  mock_service_.set_mock_delete_appcaches_for_origin_result(
      net::ERR_ABORTED);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort,
            DeleteStorageKeyData(*client, kTemp, kStorageKeyA));
  EXPECT_EQ(2, mock_service_.delete_called_count());

  Call_OnMojoDisconnect(*client);
  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, PendingRequests) {
  auto client = CreateClient();

  SetUsageMapEntry(kStorageKeyA, 1000);
  SetUsageMapEntry(kStorageKeyB, 10);
  SetUsageMapEntry(kStorageKeyOther, 500);

  // Queue up some requests.
  AsyncGetStorageKeyUsage(*client, kStorageKeyA, kTemp);
  AsyncGetStorageKeyUsage(*client, kStorageKeyB, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host());
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyOther.origin().host());
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyA);
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyB);

  EXPECT_EQ(0, num_get_storage_key_usage_completions_);
  EXPECT_EQ(0, num_get_storage_keys_completions_);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_storage_key_usage_completions_);
  EXPECT_EQ(0, num_get_storage_keys_completions_);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);

  // Pending requests should get serviced when the appcache is ready.
  Call_NotifyStorageReady(*client);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, num_get_storage_key_usage_completions_);
  EXPECT_EQ(4, num_get_storage_keys_completions_);
  EXPECT_EQ(2, num_delete_storage_keys_completions_);

  // They should be serviced in order requested.
  EXPECT_EQ(10, usage_);
  EXPECT_EQ(1ul, storage_keys_.size());
  EXPECT_THAT(storage_keys_, testing::Contains(kStorageKeyOther));

  Call_NotifyServiceDestroyed(*client);
  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyServiceWithPending) {
  auto client = CreateClient();

  SetUsageMapEntry(kStorageKeyA, 1000);
  SetUsageMapEntry(kStorageKeyB, 10);
  SetUsageMapEntry(kStorageKeyOther, 500);

  // Queue up some requests prior to being ready.
  AsyncGetStorageKeyUsage(*client, kStorageKeyA, kTemp);
  AsyncGetStorageKeyUsage(*client, kStorageKeyB, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host());
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyOther.origin().host());
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyA);
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_storage_key_usage_completions_);
  EXPECT_EQ(0, num_get_storage_keys_completions_);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);

  // Kill the service.
  Call_NotifyServiceDestroyed(*client);

  // All should have been aborted and called completion.
  EXPECT_EQ(2, num_get_storage_key_usage_completions_);
  EXPECT_EQ(4, num_get_storage_keys_completions_);
  EXPECT_EQ(2, num_delete_storage_keys_completions_);
  EXPECT_EQ(0, usage_);
  EXPECT_TRUE(storage_keys_.empty());
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnMojoDisconnect(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyQuotaManagerWithPending) {
  auto client = CreateClient();

  SetUsageMapEntry(kStorageKeyA, 1000);
  SetUsageMapEntry(kStorageKeyB, 10);
  SetUsageMapEntry(kStorageKeyOther, 500);

  // Queue up some requests prior to being ready.
  AsyncGetStorageKeyUsage(*client, kStorageKeyA, kTemp);
  AsyncGetStorageKeyUsage(*client, kStorageKeyB, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForType(*client, kTemp);
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyA.origin().host());
  AsyncGetStorageKeysForHost(*client, kTemp, kStorageKeyOther.origin().host());
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyA);
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyB);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_storage_key_usage_completions_);
  EXPECT_EQ(0, num_get_storage_keys_completions_);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);

  // Kill the quota manager.
  Call_OnMojoDisconnect(*client);
  Call_NotifyStorageReady(*client);

  // Callbacks should be deleted and not called.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, num_get_storage_key_usage_completions_);
  EXPECT_EQ(0, num_get_storage_keys_completions_);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);

  Call_NotifyServiceDestroyed(*client);
}

TEST_F(AppCacheQuotaClientTest, DestroyWithDeleteInProgress) {
  auto client = CreateClient();
  Call_NotifyStorageReady(*client);

  // Start an async delete.
  AsyncDeleteStorageKeyData(*client, kTemp, kStorageKeyB);
  EXPECT_EQ(0, num_delete_storage_keys_completions_);

  // Kill the service.
  Call_NotifyServiceDestroyed(*client);

  // Should have been aborted.
  EXPECT_EQ(1, num_delete_storage_keys_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  // A real completion callback from the service should
  // be dropped if it comes in after NotifyServiceDestroyed.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, num_delete_storage_keys_completions_);
  EXPECT_EQ(blink::mojom::QuotaStatusCode::kErrorAbort, delete_status_);

  Call_OnMojoDisconnect(*client);
}

}  // namespace content

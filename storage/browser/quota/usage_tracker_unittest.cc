// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;

namespace storage {

namespace {

void DidGetGlobalUsage(bool* done,
                       int64_t* usage_out,
                       int64_t* unlimited_usage_out,
                       int64_t usage,
                       int64_t unlimited_usage) {
  EXPECT_FALSE(*done);
  *done = true;
  *usage_out = usage;
  *unlimited_usage_out = unlimited_usage;
}

class UsageTrackerTestQuotaClient : public QuotaClient {
 public:
  UsageTrackerTestQuotaClient() = default;

  void OnQuotaManagerDestroyed() override {}

  void GetOriginUsage(const url::Origin& origin,
                      StorageType type,
                      GetOriginUsageCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    int64_t usage = GetUsage(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), usage));
  }

  void GetOriginsForType(StorageType type,
                         GetOriginsForTypeCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::vector<url::Origin> origins;
    for (const auto& origin_usage_pair : origin_usage_map_)
      origins.push_back(origin_usage_pair.first);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(origins)));
  }

  void GetOriginsForHost(StorageType type,
                         const std::string& host,
                         GetOriginsForHostCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    std::vector<url::Origin> origins;
    for (const auto& origin_usage_pair : origin_usage_map_) {
      if (origin_usage_pair.first.host() == host)
        origins.push_back(origin_usage_pair.first);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(origins)));
  }

  void DeleteOriginData(const url::Origin& origin,
                        StorageType type,
                        DeleteOriginDataCallback callback) override {
    EXPECT_EQ(StorageType::kTemporary, type);
    origin_usage_map_.erase(origin);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), QuotaStatusCode::kOk));
  }

  void PerformStorageCleanup(blink::mojom::StorageType type,
                             PerformStorageCleanupCallback callback) override {
    std::move(callback).Run();
  }

  int64_t GetUsage(const url::Origin& origin) {
    auto it = origin_usage_map_.find(origin);
    if (it == origin_usage_map_.end())
      return 0;
    return it->second;
  }

  void SetUsage(const url::Origin& origin, int64_t usage) {
    origin_usage_map_[origin] = usage;
  }

  int64_t UpdateUsage(const url::Origin& origin, int64_t delta) {
    return origin_usage_map_[origin] += delta;
  }

 private:
  ~UsageTrackerTestQuotaClient() override = default;

  std::map<url::Origin, int64_t> origin_usage_map_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTestQuotaClient);
};

}  // namespace

class UsageTrackerTest : public testing::Test {
 public:
  UsageTrackerTest()
      : storage_policy_(base::MakeRefCounted<MockSpecialStoragePolicy>()),
        quota_client_(base::MakeRefCounted<UsageTrackerTestQuotaClient>()),
        usage_tracker_(GetQuotaClientMap(),
                       StorageType::kTemporary,
                       storage_policy_.get()) {}

  ~UsageTrackerTest() override = default;

  UsageTracker* usage_tracker() {
    return &usage_tracker_;
  }

  static void DidGetUsageBreakdown(
      bool* done,
      int64_t* usage_out,
      blink::mojom::UsageBreakdownPtr* usage_breakdown_out,
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    EXPECT_FALSE(*done);
    *usage_out = usage;
    *usage_breakdown_out = std::move(usage_breakdown);
    *done = true;
  }

  void UpdateUsage(const url::Origin& origin, int64_t delta) {
    quota_client_->UpdateUsage(origin, delta);
    usage_tracker_.UpdateUsageCache(QuotaClientType::kFileSystem, origin,
                                    delta);
    base::RunLoop().RunUntilIdle();
  }

  void UpdateUsageWithoutNotification(const url::Origin& origin,
                                      int64_t delta) {
    quota_client_->UpdateUsage(origin, delta);
  }

  void GetGlobalUsage(int64_t* usage, int64_t* unlimited_usage) {
    bool done = false;
    usage_tracker_.GetGlobalUsage(
        base::BindOnce(&DidGetGlobalUsage, &done, usage, unlimited_usage));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(done);
  }

  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> GetHostUsageWithBreakdown(
      const std::string& host) {
    int64_t usage;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
    bool done = false;

    usage_tracker_.GetHostUsageWithBreakdown(
        host, base::BindOnce(&UsageTrackerTest::DidGetUsageBreakdown, &done,
                             &usage, &usage_breakdown));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(done);
    return std::make_pair(usage, std::move(usage_breakdown));
  }

  void GrantUnlimitedStoragePolicy(const url::Origin& origin) {
    if (!storage_policy_->IsStorageUnlimited(origin.GetURL())) {
      storage_policy_->AddUnlimited(origin.GetURL());
      storage_policy_->NotifyGranted(origin,
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void RevokeUnlimitedStoragePolicy(const url::Origin& origin) {
    if (storage_policy_->IsStorageUnlimited(origin.GetURL())) {
      storage_policy_->RemoveUnlimited(origin.GetURL());
      storage_policy_->NotifyRevoked(origin,
                                     SpecialStoragePolicy::STORAGE_UNLIMITED);
    }
  }

  void SetUsageCacheEnabled(const url::Origin& origin, bool enabled) {
    usage_tracker_.SetUsageCacheEnabled(QuotaClientType::kFileSystem, origin,
                                        enabled);
  }

 private:
  base::flat_map<QuotaClient*, QuotaClientType> GetQuotaClientMap() {
    base::flat_map<QuotaClient*, QuotaClientType> client_map;
    client_map.insert(
        std::make_pair(quota_client_.get(), QuotaClientType::kFileSystem));
    return client_map;
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<MockSpecialStoragePolicy> storage_policy_;
  scoped_refptr<UsageTrackerTestQuotaClient> quota_client_;
  UsageTracker usage_tracker_;

  DISALLOW_COPY_AND_ASSIGN(UsageTrackerTest);
};

TEST_F(UsageTrackerTest, GrantAndRevokeUnlimitedStorage) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(0, usage);
  EXPECT_EQ(0, unlimited_usage);

  // TODO(crbug.com/889590): Use helper for url::Origin creation from string.
  const url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  const std::string& host = origin.host();

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(100, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, CacheDisabledClientTest) {
  int64_t usage = 0;
  int64_t unlimited_usage = 0;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_expected =
      blink::mojom::UsageBreakdown::New();

  const url::Origin origin = url::Origin::Create(GURL("http://example.com"));
  const std::string& host = origin.host();

  UpdateUsage(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown_expected->fileSystem = 100;
  std::pair<int64_t, blink::mojom::UsageBreakdownPtr> host_usage_breakdown =
      GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  UpdateUsageWithoutNotification(origin, 100);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(100, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(100, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  GrantUnlimitedStoragePolicy(origin);
  UpdateUsageWithoutNotification(origin, 100);
  SetUsageCacheEnabled(origin, false);
  UpdateUsageWithoutNotification(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(400, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 400;
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  RevokeUnlimitedStoragePolicy(origin);
  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(400, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  EXPECT_EQ(400, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);

  SetUsageCacheEnabled(origin, true);
  UpdateUsage(origin, 100);

  GetGlobalUsage(&usage, &unlimited_usage);
  EXPECT_EQ(500, usage);
  EXPECT_EQ(0, unlimited_usage);
  host_usage_breakdown = GetHostUsageWithBreakdown(host);
  host_usage_breakdown_expected->fileSystem = 500;
  EXPECT_EQ(500, host_usage_breakdown.first);
  EXPECT_EQ(host_usage_breakdown_expected, host_usage_breakdown.second);
}

TEST_F(UsageTrackerTest, GlobalUsageUnlimitedUncached) {
  const url::Origin kNormal = url::Origin::Create(GURL("http://normal"));
  const url::Origin kUnlimited = url::Origin::Create(GURL("http://unlimited"));
  const url::Origin kNonCached = url::Origin::Create(GURL("http://non_cached"));
  const url::Origin kNonCachedUnlimited =
      url::Origin::Create(GURL("http://non_cached-unlimited"));

  GrantUnlimitedStoragePolicy(kUnlimited);
  GrantUnlimitedStoragePolicy(kNonCachedUnlimited);

  SetUsageCacheEnabled(kNonCached, false);
  SetUsageCacheEnabled(kNonCachedUnlimited, false);

  UpdateUsageWithoutNotification(kNormal, 1);
  UpdateUsageWithoutNotification(kUnlimited, 2);
  UpdateUsageWithoutNotification(kNonCached, 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 8);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 4 + 8, total_usage);
  EXPECT_EQ(2 + 8, unlimited_usage);

  UpdateUsageWithoutNotification(kNonCached, 16 - 4);
  UpdateUsageWithoutNotification(kNonCachedUnlimited, 32 - 8);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(1 + 2 + 16 + 32, total_usage);
  EXPECT_EQ(2 + 32, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleOriginsPerHostCachedInit) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://example.com:8080"));
  ASSERT_EQ(kOrigin1.host(), kOrigin2.host())
      << "The test assumes that the two origins have the same host";

  UpdateUsageWithoutNotification(kOrigin1, 100);
  UpdateUsageWithoutNotification(kOrigin2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the origins before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleOriginsPerHostCachedUpdate) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://example.com:8080"));
  ASSERT_EQ(kOrigin1.host(), kOrigin2.host())
      << "The test assumes that the two origins have the same host";

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the origins after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  UpdateUsage(kOrigin1, 100);
  UpdateUsage(kOrigin2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleOriginsPerHostUncachedInit) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://example.com:8080"));
  ASSERT_EQ(kOrigin1.host(), kOrigin2.host())
      << "The test assumes that the two origins have the same host";

  SetUsageCacheEnabled(kOrigin1, false);
  SetUsageCacheEnabled(kOrigin2, false);

  UpdateUsageWithoutNotification(kOrigin1, 100);
  UpdateUsageWithoutNotification(kOrigin2, 200);

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by the first call.
  // Therefore, we introduce the origins before the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

TEST_F(UsageTrackerTest, GlobalUsageMultipleOriginsPerHostUncachedUpdate) {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://example.com"));
  const url::Origin kOrigin2 =
      url::Origin::Create(GURL("http://example.com:8080"));
  ASSERT_EQ(kOrigin1.host(), kOrigin2.host())
      << "The test assumes that the two origins have the same host";

  int64_t total_usage = 0;
  int64_t unlimited_usage = 0;
  // GetGlobalUsage() takes different code paths on the first call and on
  // subsequent calls. This test covers the code path used by subsequent calls.
  // Therefore, we introduce the origins after the first call.
  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(0, total_usage);
  EXPECT_EQ(0, unlimited_usage);

  SetUsageCacheEnabled(kOrigin1, false);
  SetUsageCacheEnabled(kOrigin2, false);

  UpdateUsageWithoutNotification(kOrigin1, 100);
  UpdateUsageWithoutNotification(kOrigin2, 200);

  GetGlobalUsage(&total_usage, &unlimited_usage);
  EXPECT_EQ(100 + 200, total_usage);
  EXPECT_EQ(0, unlimited_usage);
}

}  // namespace storage

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/mock_storage_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::QuotaStatusCode;
using blink::mojom::StorageType;
using storage::QuotaClient;
using storage::QuotaManager;
using storage::UsageInfo;
using storage::UsageInfoEntries;

namespace content {

namespace {

// For shorter names.
const StorageType kTemp = StorageType::kTemporary;
const StorageType kPerm = StorageType::kPersistent;
const StorageType kSync = StorageType::kSyncable;

const int kAllClients = QuotaClient::kAllClientsMask;

// Values in bytes.
const int64_t kAvailableSpaceForApp = 13377331U;
const int64_t kMustRemainAvailableForSystem = kAvailableSpaceForApp / 2;
const int64_t kDefaultPoolSize = 1000;
const int64_t kDefaultPerHostQuota = 200;

const GURL kTestEvictionOrigin = GURL("http://test.eviction.policy/result");

// Returns a deterministic value for the amount of available disk space.
int64_t GetAvailableDiskSpaceForTest() {
  return kAvailableSpaceForApp + kMustRemainAvailableForSystem;
}

std::tuple<int64_t, int64_t> GetVolumeInfoForTests(
    const base::FilePath& unused) {
  int64_t available = static_cast<uint64_t>(GetAvailableDiskSpaceForTest());
  int64_t total = available * 2;
  return std::make_tuple(total, available);
}

// TODO(crbug.com/889590): Replace with common converter.
url::Origin ToOrigin(const std::string& url) {
  return url::Origin::Create(GURL(url));
}
}  // namespace

class QuotaManagerTest : public testing::Test {
 protected:
  using QuotaTableEntry = QuotaManager::QuotaTableEntry;
  using QuotaTableEntries = QuotaManager::QuotaTableEntries;
  using OriginInfoTableEntries = QuotaManager::OriginInfoTableEntries;

 public:
  QuotaManagerTest() : mock_time_counter_(0) {}

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    mock_special_storage_policy_ = new MockSpecialStoragePolicy;
    ResetQuotaManager(false /* is_incognito */);
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    quota_manager_ = nullptr;
    task_environment_.RunUntilIdle();
  }

 protected:
  void ResetQuotaManager(bool is_incognito) {
    quota_manager_ = new QuotaManager(is_incognito, data_dir_.GetPath(),
                                      base::ThreadTaskRunnerHandle::Get().get(),
                                      mock_special_storage_policy_.get(),
                                      storage::GetQuotaSettingsFunc());
    SetQuotaSettings(kDefaultPoolSize, kDefaultPerHostQuota,
                     is_incognito ? INT64_C(0) : kMustRemainAvailableForSystem);

    // Don't (automatically) start the eviction for testing.
    quota_manager_->eviction_disabled_ = true;
    // Don't query the hard disk for remaining capacity.
    quota_manager_->get_volume_info_fn_= &GetVolumeInfoForTests;
    additional_callback_count_ = 0;
  }

  MockStorageClient* CreateClient(
      const MockOriginData* mock_data,
      size_t mock_data_size,
      QuotaClient::ID id) {
    return new MockStorageClient(quota_manager_->proxy(),
                                 mock_data, id, mock_data_size);
  }

  void RegisterClient(MockStorageClient* client) {
    quota_manager_->proxy()->RegisterClient(client);
  }

  void GetUsageInfo() {
    usage_info_.clear();
    quota_manager_->GetUsageInfo(base::BindOnce(
        &QuotaManagerTest::DidGetUsageInfo, weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForWebApps(const url::Origin& origin, StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuotaForWebApps(
        origin, type,
        base::BindOnce(&QuotaManagerTest::DidGetUsageAndQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaWithBreakdown(const url::Origin& origin,
                                     StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    usage_breakdown_ = nullptr;
    quota_manager_->GetUsageAndQuotaWithBreakdown(
        origin, type,
        base::BindOnce(&QuotaManagerTest::DidGetUsageAndQuotaWithBreakdown,
                       weak_factory_.GetWeakPtr()));
  }

  void GetUsageAndQuotaForStorageClient(const url::Origin& origin,
                                        StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    usage_ = -1;
    quota_ = -1;
    quota_manager_->GetUsageAndQuota(
        origin, type,
        base::BindOnce(&QuotaManagerTest::DidGetUsageAndQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void SetQuotaSettings(int64_t pool_size,
                        int64_t per_host_quota,
                        int64_t must_remain_available) {
    storage::QuotaSettings settings;
    settings.pool_size = pool_size;
    settings.per_host_quota = per_host_quota;
    settings.session_only_per_host_quota =
        (per_host_quota > 0) ? (per_host_quota - 1) : 0;
    settings.must_remain_available = must_remain_available;
    settings.refresh_interval = base::TimeDelta::Max();
    quota_manager_->SetQuotaSettings(settings);
  }

  void GetPersistentHostQuota(const std::string& host) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_->GetPersistentHostQuota(
        host, base::BindOnce(&QuotaManagerTest::DidGetHostQuota,
                             weak_factory_.GetWeakPtr()));
  }

  void SetPersistentHostQuota(const std::string& host, int64_t new_quota) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_ = -1;
    quota_manager_->SetPersistentHostQuota(
        host, new_quota,
        base::BindOnce(&QuotaManagerTest::DidGetHostQuota,
                       weak_factory_.GetWeakPtr()));
  }

  void GetGlobalUsage(StorageType type) {
    usage_ = -1;
    unlimited_usage_ = -1;
    quota_manager_->GetGlobalUsage(
        type, base::BindOnce(&QuotaManagerTest::DidGetGlobalUsage,
                             weak_factory_.GetWeakPtr()));
  }

  void GetHostUsage(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_->GetHostUsage(
        host, type,
        base::BindOnce(&QuotaManagerTest::DidGetHostUsage,
                       weak_factory_.GetWeakPtr()));
  }

  void GetHostUsageBreakdown(const std::string& host, StorageType type) {
    usage_ = -1;
    quota_manager_->GetHostUsageWithBreakdown(
        host, type,
        base::BindOnce(&QuotaManagerTest::DidGetHostUsageBreakdown,
                       weak_factory_.GetWeakPtr()));
  }

  void RunAdditionalUsageAndQuotaTask(const url::Origin& origin,
                                      StorageType type) {
    quota_manager_->GetUsageAndQuota(
        origin, type,
        base::BindOnce(&QuotaManagerTest::DidGetUsageAndQuotaAdditional,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteClientOriginData(QuotaClient* client,
                              const url::Origin& origin,
                              StorageType type) {
    DCHECK(client);
    quota_status_ = QuotaStatusCode::kUnknown;
    client->DeleteOriginData(origin, type,
                             base::BindOnce(&QuotaManagerTest::StatusCallback,
                                            weak_factory_.GetWeakPtr()));
  }

  void EvictOriginData(const url::Origin& origin, StorageType type) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->EvictOriginData(
        origin, type,
        base::BindOnce(&QuotaManagerTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteOriginData(const url::Origin& origin,
                        StorageType type,
                        int quota_client_mask) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->DeleteOriginData(
        origin, type, quota_client_mask,
        base::BindOnce(&QuotaManagerTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void DeleteHostData(const std::string& host,
                      StorageType type,
                      int quota_client_mask) {
    quota_status_ = QuotaStatusCode::kUnknown;
    quota_manager_->DeleteHostData(
        host, type, quota_client_mask,
        base::BindOnce(&QuotaManagerTest::StatusCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void GetStorageCapacity() {
    available_space_ = -1;
    total_space_ = -1;
    quota_manager_->GetStorageCapacity(base::BindOnce(
        &QuotaManagerTest::DidGetStorageCapacity, weak_factory_.GetWeakPtr()));
  }

  void GetEvictionRoundInfo() {
    quota_status_ = QuotaStatusCode::kUnknown;
    settings_ = storage::QuotaSettings();
    available_space_ = -1;
    total_space_ = -1;
    usage_ = -1;
    quota_manager_->GetEvictionRoundInfo(
        base::BindOnce(&QuotaManagerTest::DidGetEvictionRoundInfo,
                       weak_factory_.GetWeakPtr()));
  }

  void GetCachedOrigins(StorageType type, std::set<url::Origin>* origins) {
    ASSERT_TRUE(origins != nullptr);
    origins->clear();
    quota_manager_->GetCachedOrigins(type, origins);
  }

  void NotifyStorageAccessed(QuotaClient* client,
                             const url::Origin& origin,
                             StorageType type) {
    DCHECK(client);
    quota_manager_->NotifyStorageAccessedInternal(
        client->id(), origin, type, IncrementMockTime());
  }

  void DeleteOriginFromDatabase(const url::Origin& origin, StorageType type) {
    quota_manager_->DeleteOriginFromDatabase(origin, type, false);
  }

  void GetEvictionOrigin(StorageType type) {
    eviction_origin_.reset();
    // The quota manager's default eviction policy is to use an LRU eviction
    // policy.
    quota_manager_->GetEvictionOrigin(
        type, 0,
        base::BindOnce(&QuotaManagerTest::DidGetEvictionOrigin,
                       weak_factory_.GetWeakPtr()));
  }

  void NotifyOriginInUse(const url::Origin& origin) {
    quota_manager_->NotifyOriginInUse(origin);
  }

  void NotifyOriginNoLongerInUse(const url::Origin& origin) {
    quota_manager_->NotifyOriginNoLongerInUse(origin);
  }

  void GetOriginsModifiedSince(StorageType type, base::Time modified_since) {
    modified_origins_.clear();
    modified_origins_type_ = StorageType::kUnknown;
    quota_manager_->GetOriginsModifiedSince(
        type, modified_since,
        base::BindOnce(&QuotaManagerTest::DidGetModifiedOrigins,
                       weak_factory_.GetWeakPtr()));
  }

  void DumpQuotaTable() {
    quota_entries_.clear();
    quota_manager_->DumpQuotaTable(base::BindOnce(
        &QuotaManagerTest::DidDumpQuotaTable, weak_factory_.GetWeakPtr()));
  }

  void DumpOriginInfoTable() {
    origin_info_entries_.clear();
    quota_manager_->DumpOriginInfoTable(base::BindOnce(
        &QuotaManagerTest::DidDumpOriginInfoTable, weak_factory_.GetWeakPtr()));
  }

  void DidGetUsageInfo(UsageInfoEntries entries) {
    usage_info_ = std::move(entries);
  }

  void DidGetUsageAndQuota(QuotaStatusCode status,
                           int64_t usage,
                           int64_t quota) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
  }

  void DidGetUsageAndQuotaWithBreakdown(
      QuotaStatusCode status,
      int64_t usage,
      int64_t quota,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    quota_status_ = status;
    usage_ = usage;
    quota_ = quota;
    usage_breakdown_ = std::move(usage_breakdown);
  }

  void DidGetQuota(QuotaStatusCode status, int64_t quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetStorageCapacity(int64_t total_space, int64_t available_space) {
    total_space_ = total_space;
    available_space_ = available_space;
  }

  void DidGetHostQuota(QuotaStatusCode status, int64_t quota) {
    quota_status_ = status;
    quota_ = quota;
  }

  void DidGetGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    usage_ = usage;
    unlimited_usage_ = unlimited_usage;
  }

  void DidGetHostUsage(int64_t usage) { usage_ = usage; }

  void StatusCallback(QuotaStatusCode status) {
    ++status_callback_count_;
    quota_status_ = status;
  }

  void DidGetHostUsageBreakdown(
      int64_t usage,
      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    usage_ = usage;
    usage_breakdown_ = std::move(usage_breakdown);
  }

  void DidGetEvictionRoundInfo(QuotaStatusCode status,
                               const storage::QuotaSettings& settings,
                               int64_t available_space,
                               int64_t total_space,
                               int64_t global_usage,
                               bool global_usage_is_complete) {
    quota_status_ = status;
    settings_ = settings;
    available_space_ = available_space;
    total_space_ = total_space;
    usage_ = global_usage;
  }

  void DidGetEvictionOrigin(const base::Optional<url::Origin>& origin) {
    eviction_origin_ = origin;
    DCHECK(!origin.has_value() || !origin->GetURL().is_empty());
  }

  void DidGetModifiedOrigins(const std::set<url::Origin>& origins,
                             StorageType type) {
    modified_origins_ = origins;
    modified_origins_type_ = type;
  }

  void DidDumpQuotaTable(const QuotaTableEntries& entries) {
    quota_entries_ = entries;
  }

  void DidDumpOriginInfoTable(const OriginInfoTableEntries& entries) {
    origin_info_entries_ = entries;
  }

  void GetUsage_WithModifyTestBody(const StorageType type);

  void set_additional_callback_count(int c) { additional_callback_count_ = c; }
  int additional_callback_count() const { return additional_callback_count_; }
  void DidGetUsageAndQuotaAdditional(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    ++additional_callback_count_;
  }

  QuotaManager* quota_manager() const { return quota_manager_.get(); }
  void set_quota_manager(QuotaManager* quota_manager) {
    quota_manager_ = quota_manager;
  }

  MockSpecialStoragePolicy* mock_special_storage_policy() const {
    return mock_special_storage_policy_.get();
  }

  QuotaStatusCode status() const { return quota_status_; }
  const UsageInfoEntries& usage_info() const { return usage_info_; }
  int64_t usage() const { return usage_; }
  const blink::mojom::UsageBreakdown& usage_breakdown() const {
    return *usage_breakdown_;
  }
  int64_t unlimited_usage() const { return unlimited_usage_; }
  int64_t quota() const { return quota_; }
  int64_t total_space() const { return total_space_; }
  int64_t available_space() const { return available_space_; }
  const base::Optional<url::Origin>& eviction_origin() const {
    return eviction_origin_;
  }
  const std::set<url::Origin>& modified_origins() const {
    return modified_origins_;
  }
  StorageType modified_origins_type() const { return modified_origins_type_; }
  const QuotaTableEntries& quota_entries() const { return quota_entries_; }
  const OriginInfoTableEntries& origin_info_entries() const {
    return origin_info_entries_;
  }
  const storage::QuotaSettings& settings() const { return settings_; }
  base::FilePath profile_path() const { return data_dir_.GetPath(); }
  int status_callback_count() const { return status_callback_count_; }
  void reset_status_callback_count() { status_callback_count_ = 0; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;

 private:
  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromDoubleT(mock_time_counter_ * 10.0);
  }

  base::ScopedTempDir data_dir_;

  scoped_refptr<QuotaManager> quota_manager_;
  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  UsageInfoEntries usage_info_;
  int64_t usage_;
  blink::mojom::UsageBreakdownPtr usage_breakdown_;
  int64_t unlimited_usage_;
  int64_t quota_;
  int64_t total_space_;
  int64_t available_space_;
  base::Optional<url::Origin> eviction_origin_;
  std::set<url::Origin> modified_origins_;
  StorageType modified_origins_type_;
  QuotaTableEntries quota_entries_;
  OriginInfoTableEntries origin_info_entries_;
  storage::QuotaSettings settings_;
  int status_callback_count_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerTest> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerTest);
};

TEST_F(QuotaManagerTest, GetUsageInfo) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",       kTemp,  10 },
    { "http://foo.com:8080/",  kTemp,  15 },
    { "http://bar.com/",       kTemp,  20 },
    { "http://bar.com/",       kPerm,  50 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",      kTemp,  30 },
    { "https://foo.com:8081/", kTemp,  35 },
    { "http://bar.com/",       kPerm,  40 },
    { "http://example.com/",   kPerm,  40 },
  };
  RegisterClient(
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem));
  RegisterClient(
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase));

  GetUsageInfo();
  task_environment_.RunUntilIdle();

  EXPECT_EQ(4U, usage_info().size());
  for (size_t i = 0; i < usage_info().size(); ++i) {
    const UsageInfo& info = usage_info()[i];
    if (info.host == "foo.com" && info.type == kTemp) {
      EXPECT_EQ(10 + 15 + 30 + 35, info.usage);
    } else if (info.host == "bar.com" && info.type == kTemp) {
      EXPECT_EQ(20, info.usage);
    } else if (info.host == "bar.com" && info.type == kPerm) {
      EXPECT_EQ(50 + 40, info.usage);
    } else if (info.host == "example.com" && info.type == kPerm) {
      EXPECT_EQ(40, info.usage);
    } else {
      ADD_FAILURE() << "Unexpected host, type: " << info.host << ", "
                    << static_cast<int>(info.type);
    }
  }
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(0, quota());
  int64_t quota_returned_for_foo = quota();

  GetUsageAndQuotaForWebApps(ToOrigin("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(quota_returned_for_foo, quota());
}

TEST_F(QuotaManagerTest, GetUsage_NoClient) {
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetUsage_EmptyClient) {
  RegisterClient(CreateClient(nullptr, 0, QuotaClient::kFileSystem));
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp,  10 },
    { "http://foo.com:8080/",   kTemp,  20 },
    { "http://bar.com/",        kTemp,   5 },
    { "https://bar.com/",       kTemp,   7 },
    { "http://baz.com/",        kTemp,  30 },
    { "http://foo.com/",        kPerm,  40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  // This time explicitly sets a temporary global quota.
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  // The host's quota should be its full portion of the global quota
  // since there's plenty of diskspace.
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerTest, GetUsage_MultipleClients) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",              kTemp, 1 },
    { "http://bar.com/",              kTemp, 2 },
    { "http://bar.com/",              kPerm, 4 },
    { "http://unlimited/",            kPerm, 8 },
  };
  static const MockOriginData kData2[] = {
    { "https://foo.com/",             kTemp, 128 },
    { "http://example.com/",          kPerm, 256 },
    { "http://unlimited/",            kTemp, 512 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetStorageCapacity();
  RegisterClient(
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem));
  RegisterClient(
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase));

  const int64_t kPoolSize = GetAvailableDiskSpaceForTest();
  const int64_t kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  EXPECT_EQ(0, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 2 + 128 + 512, usage());
  EXPECT_EQ(512, unlimited_usage());

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4 + 8 + 256, usage());
  EXPECT_EQ(8, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_Simple) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockOriginData kData1[] = {
      {"http://foo.com/", kTemp, 1}, {"http://foo.com/", kPerm, 80},
  };
  static const MockOriginData kData2[] = {
      {"http://foo.com/", kTemp, 4},
  };
  static const MockOriginData kData3[] = {
      {"http://foo.com/", kTemp, 8},
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase);
  MockStorageClient* client3 =
      CreateClient(kData3, base::size(kData3), QuotaClient::kAppcache);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  usage_breakdown_expected.fileSystem = 80;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.appcache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 4 + 8, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 4;
  usage_breakdown_expected.appcache = 8;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.appcache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_NoClient) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageBreakdown("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetHostUsageBreakdown("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_MultiOrigins) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 10}, {"http://foo.com:8080/", kTemp, 20},
      {"http://bar.com/", kTemp, 5},  {"https://bar.com/", kTemp, 7},
      {"http://baz.com/", kTemp, 30}, {"http://foo.com/", kPerm, 40},
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  usage_breakdown_expected.fileSystem = 10 + 20;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(5 + 7, usage());
  usage_breakdown_expected.fileSystem = 5 + 7;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

TEST_F(QuotaManagerTest, GetUsageWithBreakdown_MultipleClients) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  static const MockOriginData kData1[] = {
      {"http://foo.com/", kTemp, 1},
      {"http://bar.com/", kTemp, 2},
      {"http://bar.com/", kPerm, 4},
      {"http://unlimited/", kPerm, 8},
  };
  static const MockOriginData kData2[] = {
      {"https://foo.com/", kTemp, 128},
      {"http://example.com/", kPerm, 256},
      {"http://unlimited/", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  RegisterClient(
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem));
  RegisterClient(
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1 + 128, usage());
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 128;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4, usage());
  usage_breakdown_expected.fileSystem = 4;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(512, usage());
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 512;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));

  GetUsageAndQuotaWithBreakdown(ToOrigin("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(8, usage());
  usage_breakdown_expected.fileSystem = 8;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(usage_breakdown()));
}

void QuotaManagerTest::GetUsage_WithModifyTestBody(const StorageType type) {
  const MockOriginData data[] = {
    { "http://foo.com/",   type,  10 },
    { "http://foo.com:1/", type,  20 },
  };
  MockStorageClient* client =
      CreateClient(data, base::size(data), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());

  client->ModifyOriginAndNotify(ToOrigin("http://foo.com/"), type, 30);
  client->ModifyOriginAndNotify(ToOrigin("http://foo.com:1/"), type, -5);
  client->AddOriginAndNotify(ToOrigin("https://foo.com/"), type, 1);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 30 - 5 + 1, usage());
  int foo_usage = usage();

  client->AddOriginAndNotify(ToOrigin("http://bar.com/"), type, 40);
  GetUsageAndQuotaForWebApps(ToOrigin("http://bar.com/"), type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(40, usage());

  GetGlobalUsage(type);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(foo_usage + 40, usage());
  EXPECT_EQ(0, unlimited_usage());
}

TEST_F(QuotaManagerTest, GetTemporaryUsage_WithModify) {
  GetUsage_WithModifyTestBody(kTemp);
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(kPerHostQuota, quota());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kTemp, 10 },
    { "http://foo.com:8080/",   kTemp, 20 },
    { "http://bar.com/",        kTemp, 13 },
    { "http://foo.com/",        kPerm, 40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://bar.com/"), kTemp);

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(ToOrigin("http://bar.com/"), kTemp, kAllClients);

  // Nuke before waiting for callbacks.
  set_quota_manager(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const MockOriginData kData[] = {
    { "http://usage1/",    kTemp,   1 },
    { "http://usage10/",   kTemp,  10 },
    { "http://usage200/",  kTemp, 200 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));
  const int kPoolSize = 100;
  const int kPerHostQuota = 20;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  // Provided diskspace is not tight, global usage does not affect the
  // quota calculations for an individual origin, so despite global usage
  // in excess of our poolsize, we still get the nominal quota value.
  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_LE(kMustRemainAvailableForSystem, available_space());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage1/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(1, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuota, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage200/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(200, usage());
  EXPECT_EQ(kPerHostQuota, quota());  // should be clamped to the nominal quota
}

TEST_F(QuotaManagerTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const MockOriginData kData[] = {
    { "http://usage10/",   kTemp,    10 },
    { "http://usage50/",   kTemp,    50 },
    { "http://unlimited/", kTemp,  4000 },
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetStorageCapacity();
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  // Test when not overbugdet.
  const int kPerHostQuotaFor1000 = 200;
  SetQuotaSettings(1000, kPerHostQuotaFor1000, kMustRemainAvailableForSystem);
  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(4000, unlimited_usage());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor1000, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForStorageClient(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Test when overbugdet.
  const int kPerHostQuotaFor100 = 20;
  SetQuotaSettings(100, kPerHostQuotaFor100, kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForStorageClient(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(10 + 50 + 4000, usage());
  EXPECT_EQ(0, unlimited_usage());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage10/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://usage50/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(50, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());

  GetUsageAndQuotaForStorageClient(ToOrigin("http://unlimited/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(4000, usage());
  EXPECT_EQ(kPerHostQuotaFor100, quota());
}

TEST_F(QuotaManagerTest, OriginInUse) {
  const url::Origin kFooOrigin = ToOrigin("http://foo.com/");
  const url::Origin kBarOrigin = ToOrigin("http://bar.com/");

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginInUse(kFooOrigin);  // count of 2
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);  // count of 1
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kFooOrigin));

  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginInUse(kBarOrigin);
  EXPECT_TRUE(quota_manager()->IsOriginInUse(kBarOrigin));
  quota_manager()->NotifyOriginNoLongerInUse(kBarOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kBarOrigin));

  quota_manager()->NotifyOriginNoLongerInUse(kFooOrigin);
  EXPECT_FALSE(quota_manager()->IsOriginInUse(kFooOrigin));
}

TEST_F(QuotaManagerTest, GetAndSetPerststentHostQuota) {
  RegisterClient(CreateClient(nullptr, 0, QuotaClient::kFileSystem));

  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(100, quota());

  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", 200);
  GetPersistentHostQuota("foo.com");
  SetPersistentHostQuota("foo.com", QuotaManager::kPerHostPersistentQuotaLimit);
  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());

  // Persistent quota should be capped at the per-host quota limit.
  SetPersistentHostQuota("foo.com",
                         QuotaManager::kPerHostPersistentQuotaLimit + 100);
  GetPersistentHostQuota("foo.com");
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaManager::kPerHostPersistentQuotaLimit, quota());
}

TEST_F(QuotaManagerTest, GetAndSetPersistentUsageAndQuota) {
  GetStorageCapacity();
  RegisterClient(CreateClient(nullptr, 0, QuotaClient::kFileSystem));

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(100, quota());

  // The actual space avaialble is given to 'unlimited' origins as their quota.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(available_space() + usage(), quota());

  // GetUsageAndQuotaForStorageClient should just return 0 usage and
  // kNoLimit quota.
  GetUsageAndQuotaForStorageClient(ToOrigin("http://unlimited/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, usage());
  EXPECT_EQ(QuotaManager::kNoLimit, quota());
}

TEST_F(QuotaManagerTest, GetQuotaLowAvailableDiskSpace) {
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 100000},
      {"http://unlimited/", kTemp, 4000000},
  };

  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;

  // Simulating a low available disk space scenario by making
  // kMustRemainAvailable 64KB less than GetAvailableDiskSpaceForTest(), which
  // means there is 64KB of storage quota that can be used before triggering
  // the low available space logic branch in quota_manager.cc. From the
  // perspective of QuotaManager, there are 64KB of free space in the temporary
  // pool, so it should return (64KB + usage) as quota since the sum is less
  // than the default host quota.
  const int kMustRemainAvailable =
      static_cast<int>(GetAvailableDiskSpaceForTest() - 65536);
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailable);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(100000, usage());
  EXPECT_GT(kPerHostQuota, quota());
  EXPECT_EQ(65536 + usage(), quota());
}

TEST_F(QuotaManagerTest, GetStaticQuotaLowAvailableDiskSpace) {
  // This test is the same as the previous but with the kStaticHostQuota Finch
  // feature enabled. In here, we expect the low available space logic branch
  // to be ignored. Doing so should have QuotaManager return the same per host
  // quota as what is set in QuotaSettings, despite being in a state of low
  // available space. Notice the different expectation in the last line of
  // each test.
  scoped_feature_list_.InitAndEnableFeature(
      storage::features::kStaticHostQuota);
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 100000},
      {"http://unlimited/", kTemp, 4000000},
  };

  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;
  const int kMustRemainAvailable =
      static_cast<int>(GetAvailableDiskSpaceForTest() - 65536);
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailable);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(100000, usage());
  EXPECT_EQ(kPerHostQuota, quota());
}

TEST_F(QuotaManagerTest, GetSyncableQuota) {
  RegisterClient(CreateClient(nullptr, 0, QuotaClient::kFileSystem));

  // Pre-condition check: available disk space (for testing) is less than
  // the default quota for syncable storage.
  EXPECT_LE(kAvailableSpaceForApp,
            QuotaManager::kSyncableStorageDefaultHostQuota);

  // For unlimited origins the quota manager should return
  // kAvailableSpaceForApp as syncable quota (because of the pre-condition).
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  GetUsageAndQuotaForWebApps(ToOrigin("http://unlimited/"), kSync);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, usage());
  EXPECT_EQ(kAvailableSpaceForApp, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_MultiOrigins) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm, 10 },
    { "http://foo.com:8080/",   kPerm, 20 },
    { "https://foo.com/",       kPerm, 13 },
    { "https://foo.com:8081/",  kPerm, 19 },
    { "http://bar.com/",        kPerm,  5 },
    { "https://bar.com/",       kPerm,  7 },
    { "http://baz.com/",        kPerm, 30 },
    { "http://foo.com/",        kTemp, 40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  SetPersistentHostQuota("foo.com", 100);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20 + 13 + 19, usage());
  EXPECT_EQ(100, quota());
}

TEST_F(QuotaManagerTest, GetPersistentUsage_WithModify) {
  GetUsage_WithModifyTestBody(kPerm);
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_WithAdditionalTasks) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(100, quota());

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://foo.com/"), kPerm);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://bar.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10 + 20, usage());
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerTest, GetPersistentUsageAndQuota_NukeManager) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",        kPerm,  10 },
    { "http://foo.com:8080/",   kPerm,  20 },
    { "http://bar.com/",        kPerm,  13 },
    { "http://foo.com/",        kTemp,  40 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));
  SetPersistentHostQuota("foo.com", 100);

  set_additional_callback_count(0);
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://foo.com/"), kPerm);
  RunAdditionalUsageAndQuotaTask(ToOrigin("http://bar.com/"), kPerm);

  // Nuke before waiting for callbacks.
  set_quota_manager(nullptr);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, status());
}

TEST_F(QuotaManagerTest, GetUsage_Simple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20);

  GetHostUsage("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);
}

TEST_F(QuotaManagerTest, GetUsage_WithModification) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kPerm,       1 },
    { "http://foo.com:1/", kPerm,      20 },
    { "http://bar.com/",   kTemp,     300 },
    { "https://buz.com/",  kTemp,    4000 },
    { "http://buz.com/",   kTemp,   50000 },
    { "http://bar.com:1/", kPerm,  600000 },
    { "http://foo.com/",   kTemp, 7000000 },
  };

  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(ToOrigin("http://foo.com/"), kPerm, 80000000);

  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(0, unlimited_usage());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(0, unlimited_usage());

  client->ModifyOriginAndNotify(ToOrigin("http://foo.com/"), kTemp, 1);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(0, unlimited_usage());

  GetHostUsage("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000);

  client->ModifyOriginAndNotify(ToOrigin("http://buz.com/"), kTemp, 900000000);

  GetHostUsage("buz.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(usage(), 4000 + 50000 + 900000000);
}

TEST_F(QuotaManagerTest, GetUsage_WithDeleteOrigin) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteClientOriginData(client, ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetStorageCapacity) {
  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_LE(0, total_space());
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, EvictOriginData) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (size_t i = 0; i < base::size(kData1); ++i)
    quota_manager()->NotifyStorageAccessed(
        QuotaClient::kUnknown, url::Origin::Create(GURL(kData1[i].origin)),
        kData1[i].type);
  for (size_t i = 0; i < base::size(kData2); ++i)
    quota_manager()->NotifyStorageAccessed(
        QuotaClient::kUnknown, url::Origin::Create(GURL(kData2[i].origin)),
        kData2[i].type);
  task_environment_.RunUntilIdle();

  EvictOriginData(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();

  DumpOriginInfoTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : origin_info_entries()) {
    if (entry.type == kTemp)
      EXPECT_NE(std::string("http://foo.com/"), entry.origin.GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - (1 + 50000), usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, EvictOriginDataHistogram) {
  const url::Origin kOrigin = ToOrigin("http://foo.com/");
  static const MockOriginData kData[] = {
      {"http://foo.com/", kTemp, 1},
  };

  base::HistogramTester histograms;
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();

  EvictOriginData(kOrigin, kTemp);
  task_environment_.RunUntilIdle();

  // Ensure used count and time since access are recorded.
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 1);
  histograms.ExpectBucketCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 0, 1);
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginDaysSinceAccessHistogram, 1);

  // First eviction has no 'last' time to compare to.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 0);

  client->AddOriginAndNotify(kOrigin, kTemp, 100);

  // Change the used count of the origin.
  quota_manager()->NotifyStorageAccessed(QuotaClient::kUnknown, kOrigin, kTemp);
  task_environment_.RunUntilIdle();

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();

  EvictOriginData(kOrigin, kTemp);
  task_environment_.RunUntilIdle();

  // The new used count should be logged.
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 2);
  histograms.ExpectBucketCount(
      QuotaManager::kEvictedOriginAccessedCountHistogram, 1, 1);
  histograms.ExpectTotalCount(
      QuotaManager::kEvictedOriginDaysSinceAccessHistogram, 2);

  // Second eviction should log a 'time between repeated eviction' sample.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 1);

  client->AddOriginAndNotify(kOrigin, kTemp, 100);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();

  DeleteOriginFromDatabase(kOrigin, kTemp);

  // Deletion from non-eviction source should not log a histogram sample.
  histograms.ExpectTotalCount(
      QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram, 1);
}

TEST_F(QuotaManagerTest, EvictOriginDataWithDeletionError) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://bar.com/",   kTemp,    4000 },
  };
  static const int kNumberOfTemporaryOrigins = 3;
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  for (size_t i = 0; i < base::size(kData); ++i)
    NotifyStorageAccessed(client, url::Origin::Create(GURL(kData[i].origin)),
                          kData[i].type);
  task_environment_.RunUntilIdle();

  client->AddOriginToErrorSet(ToOrigin("http://foo.com/"), kTemp);

  for (int i = 0;
       i < QuotaManager::kThresholdOfErrorsToBeBlacklisted + 1;
       ++i) {
    EvictOriginData(ToOrigin("http://foo.com/"), kTemp);
    task_environment_.RunUntilIdle();
    EXPECT_EQ(QuotaStatusCode::kErrorInvalidModification, status());
  }

  DumpOriginInfoTable();
  task_environment_.RunUntilIdle();

  bool found_origin_in_database = false;
  for (const auto& entry : origin_info_entries()) {
    if (entry.type == kTemp && entry.origin == ToOrigin("http://foo.com/")) {
      found_origin_in_database = true;
      break;
    }
  }
  // The origin "http://foo.com/" should be in the database.
  EXPECT_TRUE(found_origin_in_database);

  for (size_t i = 0; i < kNumberOfTemporaryOrigins - 1; ++i) {
    GetEvictionOrigin(kTemp);
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(eviction_origin().has_value());
    // The origin "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"),
              eviction_origin()->GetURL().spec());
    DeleteOriginFromDatabase(*eviction_origin(), kTemp);
    task_environment_.RunUntilIdle();
  }

  // Now the LRU list must be empty.
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_origin().has_value());

  // Deleting origins from the database should not affect the results of the
  // following checks.
  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, GetEvictionRoundInfo) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,       1 },
    { "http://foo.com:1/", kTemp,      20 },
    { "http://foo.com/",   kPerm,     300 },
    { "http://unlimited/", kTemp,    4000 },
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  const int kPoolSize = 10000000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, kMustRemainAvailableForSystem);

  GetEvictionRoundInfo();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(21, usage());
  EXPECT_EQ(kPoolSize, settings().pool_size);
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerTest, DeleteHostDataSimple) {
  static const MockOriginData kData[] = {
    { "http://foo.com/",   kTemp,     1 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  int64_t predelete_host_pers = usage();

  DeleteHostData(std::string(), kTemp, kAllClients);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());

  DeleteHostData("foo.com", kTemp, kAllClients);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - 1, usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_tmp - 1, usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_host_pers, usage());
}

TEST_F(QuotaManagerTest, DeleteHostDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  reset_status_callback_count();
  DeleteHostData("foo.com", kTemp, kAllClients);
  DeleteHostData("bar.com", kTemp, kAllClients);
  DeleteHostData("foo.com", kTemp, kAllClients);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : origin_info_entries()) {
    if (entry.type != kTemp)
      continue;

    EXPECT_NE(std::string("http://foo.com/"), entry.origin.GetURL().spec());
    EXPECT_NE(std::string("http://foo.com:1/"), entry.origin.GetURL().spec());
    EXPECT_NE(std::string("https://foo.com/"), entry.origin.GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"), entry.origin.GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9),
            usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 20 + 50000 + 6000 + 80), usage());

  GetHostUsage("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

// Single-run DeleteOriginData cases must be well covered by
// EvictOriginData tests.
TEST_F(QuotaManagerTest, DeleteOriginDataMultiple) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp,     1 },
    { "http://foo.com:1/", kTemp,    20 },
    { "http://foo.com/",   kPerm,   300 },
    { "http://bar.com/",   kTemp,  4000 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 50000 },
    { "http://foo.com:1/", kTemp,  6000 },
    { "http://foo.com/",   kPerm,   700 },
    { "https://foo.com/",  kTemp,    80 },
    { "http://bar.com/",   kTemp,     9 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kDatabase);
  RegisterClient(client1);
  RegisterClient(client2);

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_global_tmp = usage();

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  GetHostUsage("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_tmp = usage();

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_pers = usage();

  GetHostUsage("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  const int64_t predelete_bar_pers = usage();

  for (size_t i = 0; i < base::size(kData1); ++i) {
    quota_manager()->NotifyStorageAccessed(
        QuotaClient::kUnknown, url::Origin::Create(GURL(kData1[i].origin)),
        kData1[i].type);
  }
  for (size_t i = 0; i < base::size(kData2); ++i) {
    quota_manager()->NotifyStorageAccessed(
        QuotaClient::kUnknown, url::Origin::Create(GURL(kData2[i].origin)),
        kData2[i].type);
  }
  task_environment_.RunUntilIdle();

  reset_status_callback_count();
  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp, kAllClients);
  DeleteOriginData(ToOrigin("http://bar.com/"), kTemp, kAllClients);
  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp, kAllClients);
  task_environment_.RunUntilIdle();

  EXPECT_EQ(3, status_callback_count());

  DumpOriginInfoTable();
  task_environment_.RunUntilIdle();

  for (const auto& entry : origin_info_entries()) {
    if (entry.type != kTemp)
      continue;

    EXPECT_NE(std::string("http://foo.com/"), entry.origin.GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"), entry.origin.GetURL().spec());
  }

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_global_tmp - (1 + 4000 + 50000 + 9), usage());

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - (1 + 50000), usage());

  GetHostUsage("bar.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_tmp - (4000 + 9), usage());

  GetHostUsage("foo.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_pers, usage());

  GetHostUsage("bar.com", kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_bar_pers, usage());
}

TEST_F(QuotaManagerTest, GetCachedOrigins) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,       1 },
    { "http://a.com:1/", kTemp,      20 },
    { "http://b.com/",   kPerm,     300 },
    { "http://c.com/",   kTemp,    4000 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  // TODO(kinuko): Be careful when we add cache pruner.

  std::set<url::Origin> origins;
  GetCachedOrigins(kTemp, &origins);
  EXPECT_TRUE(origins.empty());

  GetHostUsage("a.com", kTemp);
  task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(2U, origins.size());

  GetHostUsage("b.com", kTemp);
  task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(2U, origins.size());

  GetHostUsage("c.com", kTemp);
  task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  GetCachedOrigins(kPerm, &origins);
  EXPECT_TRUE(origins.empty());

  GetGlobalUsage(kTemp);
  task_environment_.RunUntilIdle();
  GetCachedOrigins(kTemp, &origins);
  EXPECT_EQ(3U, origins.size());

  for (size_t i = 0; i < base::size(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_TRUE(base::Contains(origins, ToOrigin(kData[i].origin)));
  }
}

TEST_F(QuotaManagerTest, NotifyAndLRUOrigin) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_origin().has_value());

  NotifyStorageAccessed(client, ToOrigin("http://a.com/"), kTemp);
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin()->GetURL().spec());

  NotifyStorageAccessed(client, ToOrigin("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, ToOrigin("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, ToOrigin("http://c.com/"), kTemp);
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/", eviction_origin()->GetURL().spec());

  DeleteOriginFromDatabase(*eviction_origin(), kTemp);
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("https://a.com/", eviction_origin()->GetURL().spec());

  DeleteOriginFromDatabase(*eviction_origin(), kTemp);
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://c.com/", eviction_origin()->GetURL().spec());
}

TEST_F(QuotaManagerTest, GetLRUOriginWithOriginInUse) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GURL origin;
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_origin().has_value());

  NotifyStorageAccessed(client, ToOrigin("http://a.com/"), kTemp);
  NotifyStorageAccessed(client, ToOrigin("http://b.com/"), kPerm);
  NotifyStorageAccessed(client, ToOrigin("https://a.com/"), kTemp);
  NotifyStorageAccessed(client, ToOrigin("http://c.com/"), kTemp);

  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToOrigin("http://a.com/"), *eviction_origin());

  // Notify origin http://a.com is in use.
  NotifyOriginInUse(ToOrigin("http://a.com/"));
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToOrigin("https://a.com/"), *eviction_origin());

  // Notify origin https://a.com is in use while GetEvictionOrigin is running.
  GetEvictionOrigin(kTemp);
  NotifyOriginInUse(ToOrigin("https://a.com/"));
  task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_FALSE(eviction_origin().has_value());

  // Notify access for http://c.com while GetEvictionOrigin is running.
  GetEvictionOrigin(kTemp);
  NotifyStorageAccessed(client, ToOrigin("http://c.com/"), kTemp);
  task_environment_.RunUntilIdle();
  // Post-filtering must have excluded the returned origin, so we will
  // see empty result here.
  EXPECT_FALSE(eviction_origin().has_value());

  NotifyOriginNoLongerInUse(ToOrigin("http://a.com/"));
  NotifyOriginNoLongerInUse(ToOrigin("https://a.com/"));
  GetEvictionOrigin(kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(ToOrigin("http://a.com/"), *eviction_origin());
}

TEST_F(QuotaManagerTest, GetOriginsModifiedSince) {
  static const MockOriginData kData[] = {
    { "http://a.com/",   kTemp,  0 },
    { "http://a.com:1/", kTemp,  0 },
    { "https://a.com/",  kTemp,  0 },
    { "http://b.com/",   kPerm,  0 },  // persistent
    { "http://c.com/",   kTemp,  0 },
  };
  MockStorageClient* client =
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem);
  RegisterClient(client);

  GetOriginsModifiedSince(kTemp, base::Time());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  base::Time time1 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(ToOrigin("http://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(ToOrigin("http://a.com:1/"), kTemp, 10);
  client->ModifyOriginAndNotify(ToOrigin("http://b.com/"), kPerm, 10);
  base::Time time2 = client->IncrementMockTime();
  client->ModifyOriginAndNotify(ToOrigin("https://a.com/"), kTemp, 10);
  client->ModifyOriginAndNotify(ToOrigin("http://c.com/"), kTemp, 10);
  base::Time time3 = client->IncrementMockTime();

  GetOriginsModifiedSince(kTemp, time1);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(4U, modified_origins().size());
  EXPECT_EQ(modified_origins_type(), kTemp);
  for (size_t i = 0; i < base::size(kData); ++i) {
    if (kData[i].type == kTemp)
      EXPECT_EQ(1U, modified_origins().count(ToOrigin(kData[i].origin)));
  }

  GetOriginsModifiedSince(kTemp, time2);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(2U, modified_origins().size());

  GetOriginsModifiedSince(kTemp, time3);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(modified_origins().empty());
  EXPECT_EQ(modified_origins_type(), kTemp);

  client->ModifyOriginAndNotify(ToOrigin("http://a.com/"), kTemp, 10);

  GetOriginsModifiedSince(kTemp, time3);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1U, modified_origins().size());
  EXPECT_EQ(1U, modified_origins().count(ToOrigin("http://a.com/")));
  EXPECT_EQ(modified_origins_type(), kTemp);
}

TEST_F(QuotaManagerTest, DumpQuotaTable) {
  SetPersistentHostQuota("example1.com", 1);
  SetPersistentHostQuota("example2.com", 20);
  SetPersistentHostQuota("example3.com", 300);
  task_environment_.RunUntilIdle();

  DumpQuotaTable();
  task_environment_.RunUntilIdle();

  const QuotaTableEntry kEntries[] = {
    QuotaTableEntry("example1.com", kPerm, 1),
    QuotaTableEntry("example2.com", kPerm, 20),
    QuotaTableEntry("example3.com", kPerm, 300),
  };
  std::set<QuotaTableEntry> entries(kEntries, kEntries + base::size(kEntries));

  for (const auto& quota_entry : quota_entries()) {
    SCOPED_TRACE(testing::Message() << "host = " << quota_entry.host << ", "
                                    << "quota = " << quota_entry.quota);
    EXPECT_EQ(1u, entries.erase(quota_entry));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, DumpOriginInfoTable) {
  using std::make_pair;

  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown, ToOrigin("http://example.com/"), kTemp);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown, ToOrigin("http://example.com/"), kPerm);
  quota_manager()->NotifyStorageAccessed(
      QuotaClient::kUnknown, ToOrigin("http://example.com/"), kPerm);
  task_environment_.RunUntilIdle();

  DumpOriginInfoTable();
  task_environment_.RunUntilIdle();

  using TypedOrigin = std::pair<GURL, StorageType>;
  using Entry = std::pair<TypedOrigin, int>;
  const Entry kEntries[] = {
    make_pair(make_pair(GURL("http://example.com/"), kTemp), 1),
    make_pair(make_pair(GURL("http://example.com/"), kPerm), 2),
  };
  std::set<Entry> entries(kEntries, kEntries + base::size(kEntries));

  for (const auto& origin_info : origin_info_entries()) {
    SCOPED_TRACE(testing::Message()
                 << "host = " << origin_info.origin << ", "
                 << "type = " << static_cast<int>(origin_info.type) << ", "
                 << "used_count = " << origin_info.used_count);
    EXPECT_EQ(1u, entries.erase(make_pair(
                      make_pair(origin_info.origin.GetURL(), origin_info.type),
                      origin_info.used_count)));
  }
  EXPECT_TRUE(entries.empty());
}

TEST_F(QuotaManagerTest, QuotaForEmptyHost) {
  GetPersistentHostQuota(std::string());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(0, quota());

  SetPersistentHostQuota(std::string(), 10);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kErrorNotSupported, status());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kAppcache);
  MockStorageClient* client3 =
      CreateClient(kData3, base::size(kData3), QuotaClient::kDatabase);
  MockStorageClient* client4 =
      CreateClient(kData4, base::size(kData4), QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp,
                   QuotaClient::kFileSystem);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp, QuotaClient::kAppcache);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp, QuotaClient::kDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp,
                   QuotaClient::kIndexedDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteSpecificClientTypeSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kAppcache);
  MockStorageClient* client3 =
      CreateClient(kData3, base::size(kData3), QuotaClient::kDatabase);
  MockStorageClient* client4 =
      CreateClient(kData4, base::size(kData4), QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp, QuotaClient::kFileSystem);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kAppcache);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp, QuotaClient::kIndexedDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleOrigin) {
  static const MockOriginData kData1[] = {
    { "http://foo.com/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com/",   kTemp, 8 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kAppcache);
  MockStorageClient* client3 =
      CreateClient(kData3, base::size(kData3), QuotaClient::kDatabase);
  MockStorageClient* client4 =
      CreateClient(kData4, base::size(kData4), QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp,
                   QuotaClient::kFileSystem | QuotaClient::kDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 4 - 1, usage());

  DeleteOriginData(ToOrigin("http://foo.com/"), kTemp,
                   QuotaClient::kAppcache | QuotaClient::kIndexedDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, DeleteMultipleClientTypesSingleHost) {
  static const MockOriginData kData1[] = {
    { "http://foo.com:1111/",   kTemp, 1 },
  };
  static const MockOriginData kData2[] = {
    { "http://foo.com:2222/",   kTemp, 2 },
  };
  static const MockOriginData kData3[] = {
    { "http://foo.com:3333/",   kTemp, 4 },
  };
  static const MockOriginData kData4[] = {
    { "http://foo.com:4444/",   kTemp, 8 },
  };
  MockStorageClient* client1 =
      CreateClient(kData1, base::size(kData1), QuotaClient::kFileSystem);
  MockStorageClient* client2 =
      CreateClient(kData2, base::size(kData2), QuotaClient::kAppcache);
  MockStorageClient* client3 =
      CreateClient(kData3, base::size(kData3), QuotaClient::kDatabase);
  MockStorageClient* client4 =
      CreateClient(kData4, base::size(kData4), QuotaClient::kIndexedDatabase);
  RegisterClient(client1);
  RegisterClient(client2);
  RegisterClient(client3);
  RegisterClient(client4);

  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  const int64_t predelete_foo_tmp = usage();

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kFileSystem | QuotaClient::kAppcache);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 2 - 1, usage());

  DeleteHostData("foo.com", kTemp,
      QuotaClient::kDatabase | QuotaClient::kIndexedDatabase);
  task_environment_.RunUntilIdle();
  GetHostUsage("foo.com", kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(predelete_foo_tmp - 8 - 4 - 2 - 1, usage());
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManager(true);

  static const MockOriginData kData[] = {
    { "http://foo.com/", kTemp, 10 },
    { "http://foo.com/", kPerm, 80 },
  };
  RegisterClient(
      CreateClient(kData, base::size(kData), QuotaClient::kFileSystem));

  // Query global usage to warmup the usage tracker caching.
  GetGlobalUsage(kTemp);
  GetGlobalUsage(kPerm);
  task_environment_.RunUntilIdle();

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(0, quota());

  const int kPoolSize = 1000;
  const int kPerHostQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerHostQuota, INT64_C(0));

  GetStorageCapacity();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(kPoolSize, total_space());
  EXPECT_EQ(kPoolSize - 80 - 10, available_space());

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_LE(kPerHostQuota, quota());

  mock_special_storage_policy()->AddUnlimited(GURL("http://foo.com/"));
  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(80, usage());
  EXPECT_EQ(available_space() + usage(), quota());

  GetUsageAndQuotaForWebApps(ToOrigin("http://foo.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(10, usage());
  EXPECT_EQ(available_space() + usage(), quota());
}

TEST_F(QuotaManagerTest, GetUsageAndQuota_SessionOnly) {
  const url::Origin kEpheremalOrigin = ToOrigin("http://ephemeral/");
  mock_special_storage_policy()->AddSessionOnly(kEpheremalOrigin.GetURL());

  GetUsageAndQuotaForWebApps(kEpheremalOrigin, kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(quota_manager()->settings().session_only_per_host_quota, quota());

  GetUsageAndQuotaForWebApps(kEpheremalOrigin, kPerm);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, quota());
}

}  // namespace content

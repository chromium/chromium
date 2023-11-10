// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "sql/test/test_helpers.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_internals.mojom.h"
#include "storage/browser/quota/quota_manager_impl.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/test/mock_quota_client.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom-shared.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/gurl.h"

using ::blink::StorageKey;
using ::blink::mojom::QuotaStatusCode;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

// For shorter names.
const StorageType kTemp = StorageType::kTemporary;
const StorageType kSync = StorageType::kSyncable;

const blink::mojom::StorageType kStorageTemp =
    blink::mojom::StorageType::kTemporary;
const blink::mojom::StorageType kStorageSync =
    blink::mojom::StorageType::kSyncable;

// Values in bytes.
const int64_t kAvailableSpaceForApp = 13377331U;
const int64_t kMustRemainAvailableForSystem = kAvailableSpaceForApp / 2;
const int64_t kDefaultPoolSize = 1000;
const int64_t kDefaultPerStorageKeyQuota = 200 * 1024 * 1024;
const int64_t kGigabytes = QuotaManagerImpl::kGBytes;

struct UsageAndQuotaResult {
  QuotaStatusCode status;
  int64_t usage;
  int64_t quota;
};

struct GlobalUsageResult {
  int64_t usage;
  int64_t unlimited_usage;
};

struct StorageCapacityResult {
  int64_t total_space;
  int64_t available_space;
};

struct ClientBucketData {
  const char* origin;
  std::string name;
  StorageType type;
  int64_t usage;
  int64_t quota = 0;
};

struct UsageWithBreakdown {
  int64_t usage;
  blink::mojom::UsageBreakdownPtr breakdown;
};

struct UsageAndQuotaWithBreakdown {
  QuotaStatusCode status;
  int64_t usage;
  int64_t quota;
  blink::mojom::UsageBreakdownPtr breakdown;
};

// Returns a deterministic value for the amount of available disk space.
int64_t GetAvailableDiskSpaceForTest() {
  return kAvailableSpaceForApp + kMustRemainAvailableForSystem;
}

QuotaAvailability GetVolumeInfoForTests(const base::FilePath& unused) {
  int64_t available = static_cast<uint64_t>(GetAvailableDiskSpaceForTest());
  int64_t total = available * 2;
  return QuotaAvailability(total, available);
}

StorageKey ToStorageKey(const std::string& url) {
  return StorageKey::CreateFromStringForTesting(url);
}

const storage::mojom::BucketTableEntry* FindBucketTableEntry(
    const std::vector<storage::mojom::BucketTableEntryPtr>& bucket_entries,
    BucketId& id) {
  auto it = base::ranges::find(bucket_entries, id.value(),
                               &storage::mojom::BucketTableEntry::bucket_id);
  if (it == bucket_entries.end()) {
    return nullptr;
  }
  return it->get();
}

MATCHER_P3(MatchesBucketTableEntry, storage_key, type, use_count, "") {
  return testing::ExplainMatchResult(storage_key, arg->storage_key,
                                     result_listener) &&
         testing::ExplainMatchResult(type, arg->type, result_listener) &&
         testing::ExplainMatchResult(use_count, arg->use_count,
                                     result_listener);
}

}  // namespace

class QuotaManagerImplTest : public testing::Test {
 protected:
  using BucketTableEntries = QuotaManagerImpl::BucketTableEntries;

 public:
  QuotaManagerImplTest() : mock_time_counter_(0) {}

  QuotaManagerImplTest(const QuotaManagerImplTest&) = delete;
  QuotaManagerImplTest& operator=(const QuotaManagerImplTest&) = delete;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    mock_special_storage_policy_ =
        base::MakeRefCounted<MockSpecialStoragePolicy>();
    ResetQuotaManagerImpl(false /* is_incognito */);
  }

  void TearDown() override {
    // Make sure the quota manager cleans up correctly.
    quota_manager_impl_ = nullptr;
    task_environment_.RunUntilIdle();
  }

 protected:
  void ResetQuotaManagerImpl(bool is_incognito) {
    quota_manager_impl_ = base::MakeRefCounted<QuotaManagerImpl>(
        is_incognito, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault().get(),
        /*quota_change_callback=*/base::DoNothing(),
        mock_special_storage_policy_.get(), GetQuotaSettingsFunc());
    SetQuotaSettings(kDefaultPoolSize, kDefaultPerStorageKeyQuota,
                     is_incognito ? INT64_C(0) : kMustRemainAvailableForSystem);

    // Don't (automatically) start the eviction for testing.
    quota_manager_impl_->eviction_disabled_ = true;
    // Don't query the hard disk for remaining capacity.
    quota_manager_impl_->get_volume_info_fn_ = &GetVolumeInfoForTests;
    additional_callback_count_ = 0;
  }

  MockQuotaClient* CreateAndRegisterClient(
      QuotaClientType client_type,
      const std::vector<blink::mojom::StorageType> storage_types,
      base::span<const UnmigratedStorageKeyData> unmigrated_data =
          base::span<const UnmigratedStorageKeyData>()) {
    auto mock_quota_client = std::make_unique<storage::MockQuotaClient>(
        quota_manager_impl_->proxy(), client_type, unmigrated_data);
    MockQuotaClient* mock_quota_client_ptr = mock_quota_client.get();

    mojo::PendingRemote<storage::mojom::QuotaClient> quota_client;
    mojo::MakeSelfOwnedReceiver(std::move(mock_quota_client),
                                quota_client.InitWithNewPipeAndPassReceiver());
    quota_manager_impl_->proxy()->RegisterClient(std::move(quota_client),
                                                 client_type, storage_types);
    return mock_quota_client_ptr;
  }

  // Creates buckets in QuotaDatabase if they don't exist yet, and sets usage
  // to the `client`.
  void RegisterClientBucketData(
      MockQuotaClient* client,
      base::span<const ClientBucketData> mock_data,
      std::map<BucketLocator, int64_t>* buckets_data_out = nullptr) {
    std::map<BucketLocator, int64_t> buckets_data;
    for (const ClientBucketData& data : mock_data) {
      base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
      BucketInitParams params(ToStorageKey(data.origin), data.name);
      params.quota = data.quota;
      quota_manager_impl_->GetOrCreateBucketDeprecated(params, data.type,
                                                       future.GetCallback());
      ASSERT_OK_AND_ASSIGN(auto bucket, future.Take());
      buckets_data.insert(std::pair<BucketLocator, int64_t>(
          bucket.ToBucketLocator(), data.usage));
    }
    if (buckets_data_out) {
      *buckets_data_out = buckets_data;
    }
    client->AddBucketsData(buckets_data);
  }

  void OpenDatabase() { quota_manager_impl_->EnsureDatabaseOpened(); }

  QuotaErrorOr<BucketInfo> UpdateOrCreateBucket(
      const BucketInitParams& params) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->UpdateOrCreateBucket(params, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<BucketInfo> CreateBucketForTesting(
      const StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->CreateBucketForTesting(
        storage_key, bucket_name, storage_type, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<BucketInfo> GetBucket(const StorageKey& storage_key,
                                     const std::string& bucket_name,
                                     blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->GetBucketByNameUnsafe(
        storage_key, bucket_name, storage_type, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<BucketInfo> GetBucketById(const BucketId& bucket_id) {
    base::test::TestFuture<QuotaErrorOr<BucketInfo>> future;
    quota_manager_impl_->GetBucketById(bucket_id, future.GetCallback());
    return future.Take();
  }

  std::set<StorageKey> GetStorageKeysForType(
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<std::set<StorageKey>> future;
    quota_manager_impl_->GetStorageKeysForType(
        storage_type, future.GetCallback<const std::set<StorageKey>&>());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForType(
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketInfo>>> future;
    quota_manager_impl_->GetBucketsForType(storage_type, future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForHost(
      const std::string& host,
      blink::mojom::StorageType storage_type) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketInfo>>> future;
    quota_manager_impl_->GetBucketsForHost(host, storage_type,
                                           future.GetCallback());
    return future.Take();
  }

  QuotaErrorOr<std::set<BucketInfo>> GetBucketsForStorageKey(
      const StorageKey& storage_key,
      blink::mojom::StorageType storage_type,
      bool delete_expired = false) {
    base::test::TestFuture<QuotaErrorOr<std::set<BucketInfo>>> future;
    quota_manager_impl_->GetBucketsForStorageKey(
        storage_key, storage_type, future.GetCallback(), delete_expired);
    return future.Take();
  }

  UsageAndQuotaResult GetUsageAndQuotaForWebApps(const StorageKey& storage_key,
                                                 StorageType type) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t> future;
    quota_manager_impl_->GetUsageAndQuotaForWebApps(storage_key, type,
                                                    future.GetCallback());
    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  UsageAndQuotaResult GetUsageAndQuotaForBucket(const BucketInfo& bucket_info) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t> future;
    quota_manager_impl_->GetBucketUsageAndQuota(bucket_info.id,
                                                future.GetCallback());
    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  UsageAndQuotaWithBreakdown GetUsageAndQuotaWithBreakdown(
      const StorageKey& storage_key,
      StorageType type) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t,
                           blink::mojom::UsageBreakdownPtr>
        future;
    quota_manager_impl_->GetUsageAndQuotaWithBreakdown(storage_key, type,
                                                       future.GetCallback());
    auto result = future.Take();
    return {std::get<0>(result), std::get<1>(result), std::get<2>(result),
            std::move(std::get<3>(result))};
  }

  UsageAndQuotaResult GetUsageAndQuotaForStorageClient(
      const StorageKey& storage_key,
      StorageType type) {
    base::test::TestFuture<QuotaStatusCode, int64_t, int64_t> future;
    quota_manager_impl_->GetUsageAndQuota(storage_key, type,
                                          future.GetCallback());
    return {future.Get<0>(), future.Get<1>(), future.Get<2>()};
  }

  bool CheckForSufficientSpace(const BucketLocator& bucket,
                               int64_t bytes_to_be_written) {
    base::test::TestFuture<QuotaErrorOr<int64_t>> future;
    quota_manager_impl_->GetBucketSpaceRemaining(bucket, future.GetCallback());
    auto result = future.Take();
    return result.has_value() && (result.value() >= bytes_to_be_written);
  }

  void SetQuotaSettings(int64_t pool_size,
                        int64_t per_storage_key_quota,
                        int64_t must_remain_available) {
    QuotaSettings settings;
    settings.pool_size = pool_size;
    settings.per_storage_key_quota = per_storage_key_quota;
    settings.session_only_per_storage_key_quota =
        (per_storage_key_quota > 0) ? (per_storage_key_quota - 1) : 0;
    settings.must_remain_available = must_remain_available;
    settings.refresh_interval = base::TimeDelta::Max();
    quota_manager_impl_->SetQuotaSettings(settings);
  }

  using GetVolumeInfoFn = QuotaAvailability (*)(const base::FilePath&);

  void SetGetVolumeInfoFn(GetVolumeInfoFn fn) {
    quota_manager_impl_->SetGetVolumeInfoFnForTesting(fn);
  }

  GlobalUsageResult GetGlobalUsage(StorageType type) {
    base::test::TestFuture<int64_t, int64_t> future;
    quota_manager_impl_->GetGlobalUsage(type, future.GetCallback());
    return {future.Get<0>(), future.Get<1>()};
  }

  UsageWithBreakdown GetStorageKeyUsageWithBreakdown(
      const blink::StorageKey& storage_key,
      StorageType type) {
    base::test::TestFuture<int64_t, blink::mojom::UsageBreakdownPtr> future;
    quota_manager_impl_->GetStorageKeyUsageWithBreakdown(storage_key, type,
                                                         future.GetCallback());
    auto result = future.Take();
    return {std::get<0>(result), std::move(std::get<1>(result))};
  }

  void RunAdditionalUsageAndQuotaTask(const StorageKey& storage_key,
                                      StorageType type) {
    quota_manager_impl_->GetUsageAndQuota(
        storage_key, type,
        base::BindOnce(&QuotaManagerImplTest::DidGetUsageAndQuotaAdditional,
                       weak_factory_.GetWeakPtr()));
  }

  int EvictBucketData(const BucketLocator& bucket) {
    base::test::TestFuture<int> future;
    quota_manager_impl_->EvictBucketData({bucket}, future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode DeleteBucketData(const BucketLocator& bucket,
                                   QuotaClientTypes quota_client_types) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->DeleteBucketData(bucket, std::move(quota_client_types),
                                          future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode DeleteHostData(const std::string& host, StorageType type) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->DeleteHostData(host, type, future.GetCallback());
    return future.Get();
  }

  QuotaStatusCode FindAndDeleteBucketData(const StorageKey& storage_key,
                                          const std::string& bucket_name) {
    base::test::TestFuture<QuotaStatusCode> future;
    quota_manager_impl_->FindAndDeleteBucketData(storage_key, bucket_name,
                                                 future.GetCallback());
    return future.Get();
  }

  StorageCapacityResult GetStorageCapacity() {
    base::test::TestFuture<int64_t, int64_t> future;
    quota_manager_impl_->GetStorageCapacity(future.GetCallback());
    return {future.Get<0>(), future.Get<1>()};
  }

  void GetEvictionRoundInfo() {
    quota_status_ = QuotaStatusCode::kUnknown;
    settings_ = QuotaSettings();
    available_space_ = -1;
    total_space_ = -1;
    usage_ = -1;
    quota_manager_impl_->GetEvictionRoundInfo(
        base::BindOnce(&QuotaManagerImplTest::DidGetEvictionRoundInfo,
                       weak_factory_.GetWeakPtr()));
  }

  void NotifyDefaultBucketAccessed(const StorageKey& storage_key,
                                   StorageType type,
                                   const base::Time& time) {
    auto bucket = BucketLocator::ForDefaultBucket(storage_key);
    bucket.type = type;
    quota_manager_impl_->NotifyBucketAccessed(bucket, time);
  }

  void NotifyDefaultBucketAccessed(const StorageKey& storage_key,
                                   StorageType type) {
    NotifyDefaultBucketAccessed(storage_key, type, IncrementMockTime());
  }

  void NotifyBucketAccessed(const BucketLocator& bucket) {
    quota_manager_impl_->NotifyBucketAccessed(bucket, IncrementMockTime());
  }

  void ModifyDefaultBucketAndNotify(MockQuotaClient* client,
                                    const StorageKey& storage_key,
                                    StorageType type,
                                    int delta) {
    auto bucket = BucketLocator::ForDefaultBucket(storage_key);
    bucket.type = type;
    client->ModifyBucketAndNotify(bucket, delta);
  }

  // Gets just one bucket for eviction.
  void GetEvictionBucket() {
    eviction_bucket_.reset();
    quota_manager_impl_->GetEvictionBuckets(
        /*target_usage=*/1,
        base::BindOnce(&QuotaManagerImplTest::DidGetEvictionBucket,
                       weak_factory_.GetWeakPtr()));
  }

  std::set<BucketLocator> GetEvictionBuckets(int64_t target_usage) {
    base::test::TestFuture<const std::set<BucketLocator>&> future;
    quota_manager_impl_->GetEvictionBuckets(target_usage, future.GetCallback());
    return future.Take();
  }

  std::set<BucketLocator> GetBucketsModifiedBetween(StorageType type,
                                                    base::Time begin,
                                                    base::Time end) {
    base::test::TestFuture<std::set<BucketLocator>> future;
    quota_manager_impl_->GetBucketsModifiedBetween(
        type, begin, end, future.GetCallback<const std::set<BucketLocator>&>());
    return future.Get<0>();
  }

  BucketTableEntries DumpBucketTable() {
    base::test::TestFuture<BucketTableEntries> future;
    quota_manager_impl_->DumpBucketTable(future.GetCallback());
    return future.Take();
  }

  std::vector<storage::mojom::BucketTableEntryPtr> RetrieveBucketsTable() {
    base::test::TestFuture<std::vector<storage::mojom::BucketTableEntryPtr>>
        future;
    quota_manager_impl_->RetrieveBucketsTable(future.GetCallback());
    return future.Take();
  }

  void DidGetEvictionRoundInfo(QuotaStatusCode status,
                               const QuotaSettings& settings,
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

  void DidGetEvictionBucket(const std::set<BucketLocator>& bucket) {
    if (1u == bucket.size()) {
      eviction_bucket_ = *bucket.begin();
    } else {
      EXPECT_TRUE(bucket.empty());
      eviction_bucket_ = {};
    }
  }

  void SetStoragePressureCallback(
      base::RepeatingCallback<void(const StorageKey&)> callback) {
    quota_manager_impl_->SetStoragePressureCallback(callback);
  }

  void MaybeRunStoragePressureCallback(const StorageKey& storage_key,
                                       int64_t total,
                                       int64_t available) {
    quota_manager_impl_->MaybeRunStoragePressureCallback(storage_key, total,
                                                         available);
  }

  void set_additional_callback_count(int c) { additional_callback_count_ = c; }
  int additional_callback_count() const { return additional_callback_count_; }
  void DidGetUsageAndQuotaAdditional(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    ++additional_callback_count_;
  }

  QuotaManagerImpl* quota_manager_impl() const {
    return quota_manager_impl_.get();
  }

  void set_quota_manager_impl(QuotaManagerImpl* quota_manager_impl) {
    quota_manager_impl_ = quota_manager_impl;
  }

  MockSpecialStoragePolicy* mock_special_storage_policy() const {
    return mock_special_storage_policy_.get();
  }

  std::unique_ptr<QuotaOverrideHandle> GetQuotaOverrideHandle() {
    return quota_manager_impl_->proxy()->GetQuotaOverrideHandle();
  }

  void SetQuotaChangeCallback(base::RepeatingClosure cb) {
    quota_manager_impl_->SetQuotaChangeCallbackForTesting(std::move(cb));
  }

  QuotaError CorruptDatabaseForTesting(
      base::OnceCallback<void(const base::FilePath&)> corrupter) {
    base::test::TestFuture<QuotaError> corruption_future;
    quota_manager_impl_->CorruptDatabaseForTesting(
        std::move(corrupter), corruption_future.GetCallback());
    return corruption_future.Get();
  }

  bool is_db_bootstrapping() {
    return quota_manager_impl_->is_bootstrapping_database_for_testing();
  }

  bool is_db_disabled() {
    return quota_manager_impl_->is_db_disabled_for_testing();
  }

  void DisableQuotaDatabase() {
    base::RunLoop run_loop;
    quota_manager_impl_->PostTaskAndReplyWithResultForDBThread(
        base::BindLambdaForTesting([&](QuotaDatabase* db) {
          db->SetDisabledForTesting(true);
          return QuotaError::kNone;
        }),
        base::BindLambdaForTesting([&](QuotaError error) { run_loop.Quit(); }),
        FROM_HERE, /*is_bootstrap_task=*/false);
    run_loop.Run();
  }

  void disable_database_bootstrap(bool disable) {
    quota_manager_impl_->SetBootstrapDisabledForTesting(disable);
  }

  QuotaStatusCode status() const { return quota_status_; }
  int64_t usage() const { return usage_; }
  int64_t quota() const { return quota_; }
  int64_t total_space() const { return total_space_; }
  int64_t available_space() const { return available_space_; }
  const std::optional<BucketLocator>& eviction_bucket() const {
    return eviction_bucket_;
  }
  const QuotaSettings& settings() const { return settings_; }

  void SetupQuotaManagerObserver() {
    quota_manager_observer_run_loop_ = std::make_unique<base::RunLoop>();
    quota_manager_observer_test_ =
        std::make_unique<QuotaManagerObserverTest>(weak_factory_.GetWeakPtr());
  }

  void RunUntilObserverNotifies() {
    quota_manager_observer_run_loop_->Run();
    quota_manager_observer_run_loop_ = std::make_unique<base::RunLoop>();
  }

 protected:
  enum ObserverNotifyType {
    kCreateOrUpdate,
    kDelete,
  };
  struct ObserverNotification {
    explicit ObserverNotification(BucketInfo bucket)
        : type(ObserverNotifyType::kCreateOrUpdate), bucket_info(bucket) {}
    explicit ObserverNotification(BucketLocator locator)
        : type(ObserverNotifyType::kDelete), bucket_locator(locator) {}
    ObserverNotifyType type;
    std::optional<BucketInfo> bucket_info;
    std::optional<BucketLocator> bucket_locator;
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir data_dir_;
  scoped_refptr<QuotaManagerImpl> quota_manager_impl_;
  std::vector<ObserverNotification> observer_notifications_;

 private:
  class QuotaManagerObserverTest : storage::mojom::QuotaManagerObserver {
   public:
    explicit QuotaManagerObserverTest(base::WeakPtr<QuotaManagerImplTest> owner)
        : owner_(owner) {
      owner_->quota_manager_impl_->AddObserver(
          receiver_.BindNewPipeAndPassRemote());
    }

    QuotaManagerObserverTest(const QuotaManagerObserverTest&) = delete;
    QuotaManagerObserverTest& operator=(const QuotaManagerObserverTest&) =
        delete;

    ~QuotaManagerObserverTest() override = default;

    void OnCreateOrUpdateBucket(
        const storage::BucketInfo& bucket_info) override {
      owner_->observer_notifications_.emplace_back(bucket_info);
      QuitRunLoop();
    }

    void OnDeleteBucket(const storage::BucketLocator& bucket_locator) override {
      owner_->observer_notifications_.emplace_back(bucket_locator);
      QuitRunLoop();
    }

   private:
    void QuitRunLoop() {
      if (owner_->quota_manager_observer_run_loop_) {
        owner_->quota_manager_observer_run_loop_->Quit();
      }
    }
    base::WeakPtr<QuotaManagerImplTest> owner_;
    mojo::Receiver<storage::mojom::QuotaManagerObserver> receiver_{this};
  };

  base::Time IncrementMockTime() {
    ++mock_time_counter_;
    return base::Time::FromSecondsSinceUnixEpoch(mock_time_counter_ * 10.0);
  }

  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  QuotaStatusCode quota_status_;
  int64_t usage_;
  int64_t quota_;
  int64_t total_space_;
  int64_t available_space_;
  std::optional<BucketLocator> eviction_bucket_;
  QuotaSettings settings_;
  std::unique_ptr<QuotaManagerObserverTest> quota_manager_observer_test_;
  std::unique_ptr<base::RunLoop> quota_manager_observer_run_loop_;

  int additional_callback_count_;

  int mock_time_counter_;

  base::WeakPtrFactory<QuotaManagerImplTest> weak_factory_{this};
};

TEST_F(QuotaManagerImplTest, QuotaDatabaseBootstrap) {
  static const UnmigratedStorageKeyData kData1[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 15},
      {"http://bar.com/", kSync, 50},
  };
  static const UnmigratedStorageKeyData kData2[] = {
      {"https://foo.com/", kTemp, 30},
      {"https://foo.com:8081/", kTemp, 35},
  };
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync}, kData1);
  CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp}, kData2);

  // OpenDatabase should trigger database bootstrapping.
  OpenDatabase();
  EXPECT_TRUE(is_db_bootstrapping());

  // When bootstrapping is complete, queued calls to the QuotaDatabase
  // should return successfully and buckets for registered storage keys should
  // already exist.
  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp)
          .has_value());
  EXPECT_FALSE(is_db_bootstrapping());

  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com:8080/"), kDefaultBucketName, kTemp)
          .has_value());

  ASSERT_TRUE(GetBucket(ToStorageKey("https://foo.com:8081/"),
                        kDefaultBucketName, kTemp)
                  .has_value());

  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://bar.com/"), kDefaultBucketName, kSync)
          .has_value());
}

TEST_F(QuotaManagerImplTest, CorruptionRecovery) {
  // Setup clients with both unmigrated and migrated data. Before corruption the
  // bucket data will be used, while after corruption recovery data should be
  // migrated again.
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 15},
  };
  static const UnmigratedStorageKeyData kUnmigratedData1[] = {
      {"http://foo.com/", kTemp, 10},
      {"http://foo.com:8080/", kTemp, 15},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 30},
      {"https://foo.com:8081/", kDefaultBucketName, kTemp, 35},
  };
  static const UnmigratedStorageKeyData kUnmigratedData2[] = {
      {"https://foo.com/", kTemp, 30},
      {"https://foo.com:8081/", kTemp, 35},
  };
  MockQuotaClient* fs_client = CreateAndRegisterClient(
      QuotaClientType::kFileSystem, {kTemp, kSync}, kUnmigratedData1);
  MockQuotaClient* database_client = CreateAndRegisterClient(
      QuotaClientType::kDatabase, {kTemp}, kUnmigratedData2);
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(database_client, kData2);

  // Basic sanity checks, make sure setup worked correctly.
  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp)
          .has_value());
  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com:8080/"), kDefaultBucketName, kTemp)
          .has_value());
  ASSERT_TRUE(GetBucket(ToStorageKey("https://foo.com:8081/"),
                        kDefaultBucketName, kTemp)
                  .has_value());

  // Corrupt the database to make bucket lookup fail.
  QuotaError corruption_error = CorruptDatabaseForTesting(
      base::BindOnce([](const base::FilePath& db_path) {
        ASSERT_TRUE(
            sql::test::CorruptIndexRootPage(db_path, "buckets_by_storage_key"));
      }));
  ASSERT_EQ(QuotaError::kNone, corruption_error);

  // Try to lookup a bucket, this should report a failure.
  EXPECT_FALSE(quota_manager_impl_->is_db_disabled_for_testing());
  EXPECT_FALSE(is_db_bootstrapping());

  EXPECT_THAT(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp),
      base::test::ErrorIs(QuotaError::kDatabaseError));

  // The last lookup attempt should have started another bootstrap attempt.
  EXPECT_TRUE(is_db_bootstrapping());

  // And with that bucket lookup should be working again.
  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp)
          .has_value());
}

TEST_F(QuotaManagerImplTest, GetUsageInfo) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 15},
      {"http://bar.com/", "logs", kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kSync, 50},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 30},
      {"https://foo.com:8081/", kDefaultBucketName, kTemp, 35},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* database_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(database_client, kData2);

  base::test::TestFuture<UsageInfoEntries> future;
  quota_manager_impl()->GetUsageInfo(future.GetCallback());
  auto entries = future.Get();

  EXPECT_THAT(entries, testing::UnorderedElementsAre(
                           UsageInfo("foo.com", kTemp, 10 + 15 + 30 + 35),
                           UsageInfo("bar.com", kTemp, 20),
                           UsageInfo("bar.com", kSync, 50)));
}

TEST_F(QuotaManagerImplTest, UpdateUsageInfo) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://bar.com/", kDefaultBucketName, kTemp, 50},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  std::map<BucketLocator, int64_t> buckets_data;
  RegisterClientBucketData(fs_client, kData1, &buckets_data);
  ASSERT_EQ(buckets_data.size(), 2u);
  BucketLocator first_bucket_locator = buckets_data.begin()->first;

  {
    base::test::TestFuture<UsageInfoEntries> future;
    quota_manager_impl()->GetUsageInfo(future.GetCallback());
    auto entries = future.Get();

    EXPECT_THAT(entries,
                testing::UnorderedElementsAre(UsageInfo("foo.com", kTemp, 10),
                                              UsageInfo("bar.com", kTemp, 50)));

    // The quota client was queried once for each bucket.
    EXPECT_EQ(2U, fs_client->get_bucket_usage_call_count());
  }

  // Notify of a change with a provided byte delta.
  quota_manager_impl()->NotifyBucketModified(
      QuotaClientType::kFileSystem, first_bucket_locator, /*delta=*/7,
      base::Time::Now(), base::DoNothing());

  {
    base::test::TestFuture<UsageInfoEntries> future;
    quota_manager_impl()->GetUsageInfo(future.GetCallback());
    auto entries = future.Get();

    EXPECT_THAT(entries,
                testing::UnorderedElementsAre(UsageInfo("foo.com", kTemp, 17),
                                              UsageInfo("bar.com", kTemp, 50)));

    // The quota client was not queried any more times since the values were
    // cached and then updated.
    EXPECT_EQ(2U, fs_client->get_bucket_usage_call_count());
  }

  // Dirty the cache by passing a null delta.
  quota_manager_impl()->NotifyBucketModified(
      QuotaClientType::kFileSystem, first_bucket_locator,
      /*delta=*/std::nullopt, base::Time::Now(), base::DoNothing());

  {
    base::test::TestFuture<UsageInfoEntries> future;
    quota_manager_impl()->GetUsageInfo(future.GetCallback());
    auto entries = future.Get();

    // Since the cache was tossed out, the mock quota client is consulted again
    // for its usage.
    EXPECT_THAT(entries,
                testing::UnorderedElementsAre(UsageInfo("foo.com", kTemp, 10),
                                              UsageInfo("bar.com", kTemp, 50)));

    // The quota client was queried one more time.
    EXPECT_EQ(3U, fs_client->get_bucket_usage_call_count());
  }
}

TEST_F(QuotaManagerImplTest, UpdateOrCreateBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  ASSERT_OK_AND_ASSIGN(auto bucket,
                       UpdateOrCreateBucket({storage_key, bucket_name}));

  BucketId created_bucket_id = bucket.id;

  EXPECT_THAT(UpdateOrCreateBucket({storage_key, bucket_name}),
              base::test::ValueIs(
                  ::testing::Field(&BucketInfo::id, created_bucket_id)));
}

TEST_F(QuotaManagerImplTest, UpdateOrCreateBucket_Expiration) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  QuotaDatabase::SetClockForTesting(clock.get());
  clock->SetNow(base::Time::Now());

  BucketInitParams params(ToStorageKey("http://a.com/"), "bucket_a");
  params.expiration = clock->Now() - base::Days(1);

  ASSERT_FALSE(UpdateOrCreateBucket(params).has_value());

  // Create a new bucket.
  params.expiration = clock->Now() + base::Days(1);
  params.quota = 1000;
  ASSERT_OK_AND_ASSIGN(auto bucket, UpdateOrCreateBucket(params));
  EXPECT_EQ(bucket.expiration, params.expiration);
  EXPECT_EQ(bucket.quota, 1000);

  // Get/Update the same bucket. Verify expiration is updated, but quota is not.
  params.expiration = clock->Now() + base::Days(5);
  params.quota = 500;
  ASSERT_OK_AND_ASSIGN(bucket, UpdateOrCreateBucket(params));
  EXPECT_EQ(bucket.expiration, params.expiration);
  EXPECT_EQ(bucket.quota, 1000);

  // Verify that the bucket is clobbered due to being expired. In this case, the
  // new quota is respected.
  clock->Advance(base::Days(20));
  params.expiration = base::Time();
  ASSERT_OK_AND_ASSIGN(bucket, UpdateOrCreateBucket(params));
  EXPECT_EQ(bucket.expiration, params.expiration);
  EXPECT_EQ(bucket.quota, 500);

  QuotaDatabase::SetClockForTesting(nullptr);
}

TEST_F(QuotaManagerImplTest, UpdateOrCreateBucket_Overflow) {
  const int kPoolSize = 100;
  // This quota for the storage key implies only two buckets can be constructed.
  const int kPerStorageKeyQuota = 40 * 1024 * 1024;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  StorageKey storage_key = ToStorageKey("http://a.com/");

  EXPECT_TRUE(UpdateOrCreateBucket({storage_key, "bucket_a"}).has_value());
  EXPECT_TRUE(UpdateOrCreateBucket({storage_key, "bucket_b"}).has_value());
  EXPECT_THAT(UpdateOrCreateBucket({storage_key, "bucket_c"}),
              base::test::ErrorIs(QuotaError::kQuotaExceeded));

  // Default bucket shouldn't be limited by the quota.
  EXPECT_TRUE(
      UpdateOrCreateBucket({storage_key, kDefaultBucketName}).has_value());
}

// Make sure `EvictExpiredBuckets` deletes expired buckets.
TEST_F(QuotaManagerImplTest, EvictExpiredBuckets) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  QuotaDatabase::SetClockForTesting(clock.get());
  clock->SetNow(base::Time::Now());

  BucketInitParams params(ToStorageKey("http://a.com/"), "bucket_a");
  params.expiration = clock->Now() + base::Days(1);
  ASSERT_OK_AND_ASSIGN(auto bucket, UpdateOrCreateBucket(params));

  BucketInitParams params_b(ToStorageKey("http://b.com/"), "bucket_b");
  params_b.expiration = clock->Now() + base::Days(10);
  ASSERT_OK_AND_ASSIGN(auto bucket_b, UpdateOrCreateBucket(params_b));

  // No specified expiration.
  BucketInitParams params_c(ToStorageKey("http://c.com/"), "bucket_c");
  ASSERT_OK_AND_ASSIGN(auto bucket_c, UpdateOrCreateBucket(params_c));

  clock->Advance(base::Days(5));

  // Evict expired buckets.
  base::test::TestFuture<QuotaStatusCode> future;
  quota_manager_impl_->EvictExpiredBuckets(future.GetCallback());
  EXPECT_EQ(QuotaStatusCode::kOk, future.Get());

  EXPECT_FALSE(GetBucketById(bucket.id).has_value());
  EXPECT_TRUE(GetBucketById(bucket_b.id).has_value());
  EXPECT_TRUE(GetBucketById(bucket_c.id).has_value());

  QuotaDatabase::SetClockForTesting(nullptr);
}

TEST_F(QuotaManagerImplTest, GetOrCreateBucketSync) {
  base::RunLoop loop;
  // Post the function call on a different thread to ensure that the
  // production DCHECK in GetOrCreateBucketSync passes.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()}, base::BindLambdaForTesting([&]() {
        base::ScopedAllowBaseSyncPrimitivesForTesting allow;
        BucketInitParams params(ToStorageKey("http://b.com"), "bucket_b");
        // Ensure that the synchronous function returns a bucket.
        ASSERT_OK_AND_ASSIGN(
            auto bucket,
            quota_manager_impl_->proxy()->GetOrCreateBucketSync(params));
        BucketId created_bucket_id = bucket.id;

        // Ensure that the synchronous function does not create a new bucket
        // each time.
        ASSERT_OK_AND_ASSIGN(
            bucket,
            quota_manager_impl_->proxy()->GetOrCreateBucketSync(params));
        EXPECT_EQ(bucket.id, created_bucket_id);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(QuotaManagerImplTest, GetBucket) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  ASSERT_OK_AND_ASSIGN(BucketInfo created_bucket,
                       CreateBucketForTesting(storage_key, bucket_name, kTemp));

  ASSERT_OK_AND_ASSIGN(BucketInfo retrieved_bucket,
                       GetBucket(storage_key, bucket_name, kTemp));
  EXPECT_EQ(created_bucket.id, retrieved_bucket.id);

  EXPECT_THAT(GetBucket(storage_key, "bucket_b", kTemp),
              base::test::ErrorIs(QuotaError::kNotFound));
  ASSERT_FALSE(is_db_disabled());
}

TEST_F(QuotaManagerImplTest, GetBucketById) {
  StorageKey storage_key = ToStorageKey("http://a.com/");
  std::string bucket_name = "bucket_a";

  ASSERT_OK_AND_ASSIGN(BucketInfo created_bucket,
                       CreateBucketForTesting(storage_key, bucket_name, kTemp));

  ASSERT_OK_AND_ASSIGN(BucketInfo retrieved_bucket,
                       GetBucketById(created_bucket.id));
  EXPECT_EQ(created_bucket.id, retrieved_bucket.id);

  EXPECT_THAT(GetBucketById(BucketId::FromUnsafeValue(0)),
              base::test::ErrorIs(QuotaError::kNotFound));
  ASSERT_FALSE(is_db_disabled());
}

TEST_F(QuotaManagerImplTest, GetStorageKeysForType) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_a,
      CreateBucketForTesting(storage_key_a, "bucket_a", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_b,
      CreateBucketForTesting(storage_key_b, "bucket_b", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_c,
      CreateBucketForTesting(storage_key_c, kDefaultBucketName, kSync));

  EXPECT_THAT(GetStorageKeysForType(kTemp),
              testing::UnorderedElementsAre(storage_key_a, storage_key_b));

  EXPECT_THAT(GetStorageKeysForType(kSync),
              testing::UnorderedElementsAre(storage_key_c));
}

TEST_F(QuotaManagerImplTest, GetStorageKeysForTypeWithDatabaseError) {
  disable_database_bootstrap(true);
  OpenDatabase();

  // Disable quota database for database error behavior.
  DisableQuotaDatabase();

  // Return empty set when error is encountered.
  EXPECT_TRUE(GetStorageKeysForType(kTemp).empty());
}

TEST_F(QuotaManagerImplTest, QuotaDatabaseResultHistogram) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 123},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);
  base::HistogramTester histograms;

  ASSERT_TRUE(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp)
          .has_value());

  histograms.ExpectTotalCount("Quota.QuotaDatabaseError",
                              /*expected_count=*/0);

  // Corrupt QuotaDatabase so any future request returns a QuotaError.
  QuotaError corruption_error = CorruptDatabaseForTesting(
      base::BindOnce([](const base::FilePath& db_path) {
        ASSERT_TRUE(
            sql::test::CorruptIndexRootPage(db_path, "buckets_by_storage_key"));
      }));
  ASSERT_EQ(QuotaError::kNone, corruption_error);

  // Refetching the bucket with a corrupted database should return an error.
  EXPECT_THAT(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp),
      base::test::ErrorIs(QuotaError::kDatabaseError));

  histograms.ExpectTotalCount("Quota.QuotaDatabaseError",
                              /*expected_count=*/1);
}

TEST_F(QuotaManagerImplTest, GetBucketsForType) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_a,
      CreateBucketForTesting(storage_key_a, "bucket_a", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_b,
      CreateBucketForTesting(storage_key_b, "bucket_b", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_c,
      CreateBucketForTesting(storage_key_c, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets, GetBucketsForType(kTemp));
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_a));
  EXPECT_THAT(buckets, testing::Contains(bucket_b));

  ASSERT_OK_AND_ASSIGN(buckets, GetBucketsForType(kSync));
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_c));
}

TEST_F(QuotaManagerImplTest, GetBucketsForHost) {
  StorageKey host_a_storage_key_1 = ToStorageKey("http://a.com/");
  StorageKey host_a_storage_key_2 = ToStorageKey("https://a.com:123/");
  StorageKey host_b_storage_key = ToStorageKey("http://b.com/");

  ASSERT_OK_AND_ASSIGN(
      BucketInfo host_a_bucket_1,
      CreateBucketForTesting(host_a_storage_key_1, kDefaultBucketName, kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo host_a_bucket_2,
      CreateBucketForTesting(host_a_storage_key_2, "test", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo host_b_bucket,
      CreateBucketForTesting(host_b_storage_key, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                       GetBucketsForHost("a.com", kTemp));
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(host_a_bucket_1));
  EXPECT_THAT(buckets, testing::Contains(host_a_bucket_2));

  ASSERT_OK_AND_ASSIGN(buckets, GetBucketsForHost("b.com", kSync));
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(host_b_bucket));
}

TEST_F(QuotaManagerImplTest, GetBucketsForStorageKey) {
  StorageKey storage_key_a = ToStorageKey("http://a.com/");
  StorageKey storage_key_b = ToStorageKey("http://b.com/");
  StorageKey storage_key_c = ToStorageKey("http://c.com/");

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_a1,
      CreateBucketForTesting(storage_key_a, "bucket_a1", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_a2,
      CreateBucketForTesting(storage_key_a, "bucket_a2", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_b,
      CreateBucketForTesting(storage_key_b, "bucket_b", kTemp));

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket_c,
      CreateBucketForTesting(storage_key_c, kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(std::set<BucketInfo> buckets,
                       GetBucketsForStorageKey(storage_key_a, kTemp));
  EXPECT_EQ(2U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_a1));
  EXPECT_THAT(buckets, testing::Contains(bucket_a2));

  ASSERT_OK_AND_ASSIGN(buckets, GetBucketsForStorageKey(storage_key_a, kSync));
  EXPECT_TRUE(buckets.empty());

  ASSERT_OK_AND_ASSIGN(buckets, GetBucketsForStorageKey(storage_key_c, kSync));
  EXPECT_EQ(1U, buckets.size());
  EXPECT_THAT(buckets, testing::Contains(bucket_c));
}

TEST_F(QuotaManagerImplTest, GetBucketsForStorageKey_Expiration) {
  StorageKey storage_key = ToStorageKey("http://a.com/");

  auto clock = std::make_unique<base::SimpleTestClock>();
  QuotaDatabase::SetClockForTesting(clock.get());
  clock->SetNow(base::Time::Now());

  BucketInitParams params(storage_key, "bucket_1");
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket_1, UpdateOrCreateBucket(params));

  params.name = "bucket_2";
  params.expiration = clock->Now() + base::Days(1);
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket_2, UpdateOrCreateBucket(params));

  params.name = "bucket_3";
  ASSERT_OK_AND_ASSIGN(BucketInfo bucket_3, UpdateOrCreateBucket(params));

  clock->Advance(base::Days(2));

  ASSERT_OK_AND_ASSIGN(
      std::set<BucketInfo> buckets,
      GetBucketsForStorageKey(storage_key, kTemp, /*delete_expired=*/true));
  ASSERT_EQ(1U, buckets.size());
  EXPECT_EQ(*buckets.begin(), bucket_1);

  QuotaDatabase::SetClockForTesting(nullptr);
}

TEST_F(QuotaManagerImplTest, EnforceQuota) {
  const int kPoolSize = 10000;
  const int kPerStorageKeyQuota = 5000;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  static const ClientBucketData kData[] = {
      {"https://foo.com/", "logs", kTemp, /*usage=*/1000, /*quota=*/1025},
      {
          "https://foo.com/",
          "cache",
          kTemp,
          /*usage=*/0,
      },
      {"https://foo.com/", kDefaultBucketName, kTemp, /*usage=*/3900},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  // Check a non-default bucket's custom quota is enforced.
  auto logs_bucket = GetBucket(ToStorageKey("https://foo.com/"), "logs", kTemp);
  EXPECT_TRUE(CheckForSufficientSpace(logs_bucket->ToBucketLocator(), 20));
  EXPECT_FALSE(CheckForSufficientSpace(logs_bucket->ToBucketLocator(), 26));

  // Check the StorageKey quota is enforced for a non-default bucket.
  auto cache_bucket =
      GetBucket(ToStorageKey("https://foo.com/"), "cache", kTemp);
  EXPECT_TRUE(CheckForSufficientSpace(cache_bucket->ToBucketLocator(), 75));
  EXPECT_FALSE(CheckForSufficientSpace(cache_bucket->ToBucketLocator(), 200));

  // Check the StorageKeyQuota is enforced for a default bucket.
  BucketLocator default_bucket =
      BucketLocator::ForDefaultBucket(ToStorageKey("https://foo.com/"));
  EXPECT_TRUE(CheckForSufficientSpace(default_bucket, 75));
  EXPECT_FALSE(CheckForSufficientSpace(default_bucket, 200));
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Simple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", "logs", kTemp, 10},
      {"http://foo.com/", kDefaultBucketName, kSync, 80},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 80);
  EXPECT_GT(result.quota, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_GT(result.quota, 0);
  int64_t quota_returned_for_foo = result.quota;

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, quota_returned_for_foo);
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_SingleBucket) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", "logs", kTemp, 10},
      {"http://foo.com/", "inbox", kTemp, 60},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});

  // Initialize the logs bucket with a non-default quota.
  BucketInitParams params(ToStorageKey("http://foo.com/"), "logs");
  params.quota = 117;
  ASSERT_TRUE(UpdateOrCreateBucket(params).has_value());

  RegisterClientBucketData(fs_client, kData);

  {
    ASSERT_OK_AND_ASSIGN(
        BucketInfo bucket,
        UpdateOrCreateBucket({ToStorageKey("http://foo.com/"), "logs"}));
    auto result = GetUsageAndQuotaForBucket(bucket);
    EXPECT_EQ(result.status, QuotaStatusCode::kOk);
    EXPECT_EQ(result.usage, 10);
    EXPECT_EQ(result.quota, params.quota);
  }

  {
    ASSERT_OK_AND_ASSIGN(
        BucketInfo bucket,
        UpdateOrCreateBucket({ToStorageKey("http://foo.com/"), "inbox"}));
    auto result = GetUsageAndQuotaForBucket(bucket);
    EXPECT_EQ(result.status, QuotaStatusCode::kOk);
    EXPECT_EQ(result.usage, 60);
    EXPECT_EQ(result.quota, kDefaultPerStorageKeyQuota);
  }
}

TEST_F(QuotaManagerImplTest, GetUsage_NoClient) {
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);
  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
             .usage);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetUsage_EmptyClient) {
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);
  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
             .usage);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 0);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_MultiStorageKeys) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", "logs", kTemp, 5},
      {"https://bar.com/", "notes", kTemp, 7},
      {"http://baz.com/", "songs", kTemp, 30},
      {"http://foo.com/", kDefaultBucketName, kSync, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  // This time explicitly sets a temporary global quota.
  const int kPoolSize = 100;
  const int kPerStorageKeyQuota = 20;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com:8080/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 20);

  // The host's quota should be its full portion of the global quota
  // since there's plenty of diskspace.
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 5);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("https://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 7);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, GetUsage_MultipleClients) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 2},
      {"http://bar.com/", kDefaultBucketName, kSync, 4},
      {"http://unlimited/", kDefaultBucketName, kSync, 8},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 128},
      {"http://unlimited/", "logs", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto storage_capacity = GetStorageCapacity();

  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* database_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(database_client, kData2);

  const int64_t kPoolSize = GetAvailableDiskSpaceForTest();
  const int64_t kPerStorageKeyQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 1);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("https://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 128);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4);
  EXPECT_EQ(result.quota,
            QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 512);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 8);
  EXPECT_EQ(result.quota,
            QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 1 + 2 + 128 + 512);
  EXPECT_EQ(global_usage_result.unlimited_usage, 512);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 4 + 8);
  // Syncable storage should always enforce quota.
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_Simple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com/", kDefaultBucketName, kSync, 80},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);
  RegisterClientBucketData(sw_client, kData3);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  auto result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(80, result.usage);
  usage_breakdown_expected.fileSystem = 80;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.serviceWorkerCache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(1 + 4 + 8, result.usage);
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 4;
  usage_breakdown_expected.serviceWorkerCache = 8;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(0, result.usage);
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 0;
  usage_breakdown_expected.serviceWorkerCache = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_NoClient) {
  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();

  auto result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(0, result.usage);
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(0, result.usage);
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  auto usage =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(0, usage.usage);
  EXPECT_TRUE(usage_breakdown_expected.Equals(*usage.breakdown));

  usage =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync);
  EXPECT_EQ(0, usage.usage);
  EXPECT_TRUE(usage_breakdown_expected.Equals(*usage.breakdown));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultiStorageKeys) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", "logs", kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 5},
      {"https://bar.com/", kDefaultBucketName, kTemp, 7},
      {"http://baz.com/", "logs", kTemp, 30},
      {"http://foo.com/", kDefaultBucketName, kSync, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  auto result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(10, result.usage);
  usage_breakdown_expected.fileSystem = 10;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
  result = GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com:8080/"),
                                         kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(20, result.usage);
  usage_breakdown_expected.fileSystem = 20;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(5, result.usage);
  usage_breakdown_expected.fileSystem = 5;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("https://bar.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(7, result.usage);
  usage_breakdown_expected.fileSystem = 7;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
}

TEST_F(QuotaManagerImplTest, GetUsageWithBreakdown_MultipleClients) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 2},
      {"http://bar.com/", kDefaultBucketName, kSync, 4},
      {"http://unlimited/", kDefaultBucketName, kSync, 8},
  };
  static const ClientBucketData kData2[] = {
      {"https://foo.com/", kDefaultBucketName, kTemp, 128},
      {"http://unlimited/", "logs", kTemp, 512},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  blink::mojom::UsageBreakdown usage_breakdown_expected =
      blink::mojom::UsageBreakdown();
  auto result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(1, result.usage);
  usage_breakdown_expected.fileSystem = 1;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("https://foo.com/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(128, result.usage);
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 128;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://bar.com/"), kSync);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(4, result.usage);
  usage_breakdown_expected.fileSystem = 4;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(512, result.usage);
  usage_breakdown_expected.fileSystem = 0;
  usage_breakdown_expected.webSql = 512;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));

  result =
      GetUsageAndQuotaWithBreakdown(ToStorageKey("http://unlimited/"), kSync);
  EXPECT_EQ(QuotaStatusCode::kOk, result.status);
  EXPECT_EQ(8, result.usage);
  usage_breakdown_expected.fileSystem = 8;
  usage_breakdown_expected.webSql = 0;
  EXPECT_TRUE(usage_breakdown_expected.Equals(*result.breakdown));
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsage_WithModify) {
  const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://bar.com/", kDefaultBucketName, kTemp, 0},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"https://foo.com/", kDefaultBucketName, kTemp, 0},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(client, kData);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com:1/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 20);

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://foo.com/"), kTemp,
                               30);
  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://foo.com:1/"), kTemp,
                               -5);
  ModifyDefaultBucketAndNotify(client, ToStorageKey("https://foo.com/"), kTemp,
                               1);

  // Database call to ensure modification calls have completed.
  std::ignore =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10 + 30);
  int foo_usage = result.usage;
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com:1/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 20 - 5);
  int foo1_usage = result.usage;

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://bar.com/"), kTemp,
                               40);

  // Database call to ensure modification calls have completed.
  std::ignore =
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://bar.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 40);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, foo_usage + foo1_usage + 40 + 1);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_WithAdditionalTasks) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", "logs", kTemp, 13},
      {"http://foo.com/", kDefaultBucketName, kSync, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerStorageKeyQuota = 20;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
  result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com:8080/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 20);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  set_additional_callback_count(0);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);
  task_environment_.RunUntilIdle();
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
  EXPECT_EQ(2, additional_callback_count());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_NukeManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
      {"http://foo.com:8080/", kDefaultBucketName, kTemp, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 13},
      {"http://foo.com/", kDefaultBucketName, kSync, 40},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerStorageKeyQuota = 20;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  set_additional_callback_count(0);

  GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://foo.com/"), kTemp);
  RunAdditionalUsageAndQuotaTask(ToStorageKey("http://bar.com/"), kTemp);

  base::test::TestFuture<QuotaStatusCode> future_foo;
  base::test::TestFuture<QuotaStatusCode> future_bar;
  quota_manager_impl()->DeleteHostData("foo.com", kTemp,
                                       future_foo.GetCallback());
  quota_manager_impl()->DeleteHostData("bar.com", kTemp,
                                       future_bar.GetCallback());

  // Nuke before waiting for callbacks.
  set_quota_manager_impl(nullptr);

  EXPECT_EQ(QuotaStatusCode::kErrorAbort, future_foo.Get());
  EXPECT_EQ(QuotaStatusCode::kErrorAbort, future_bar.Get());
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Overbudget) {
  static const ClientBucketData kData[] = {
      {"http://usage1/", kDefaultBucketName, kTemp, 1},
      {"http://usage10/", kDefaultBucketName, kTemp, 10},
      {"http://usage200/", kDefaultBucketName, kTemp, 200},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 100;
  const int kPerStorageKeyQuota = 20;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  // Provided diskspace is not tight, global usage does not affect the
  // quota calculations for an individual storage key, so despite global usage
  // in excess of our poolsize, we still get the nominal quota value.
  auto storage_capacity = GetStorageCapacity();
  EXPECT_LE(kMustRemainAvailableForSystem, storage_capacity.available_space);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://usage1/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 1);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage200/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 200);
  // Should be clamped to the nominal quota.
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, GetTemporaryUsageAndQuota_Unlimited) {
  static const ClientBucketData kData[] = {
      {"http://usage10/", kDefaultBucketName, kTemp, 10},
      {"http://usage50/", kDefaultBucketName, kTemp, 50},
      {"http://unlimited/", "inbox", kTemp, 4000},
  };
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto storage_capacity = GetStorageCapacity();
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  // Test when not overbugdet.
  const int kPerStorageKeyQuotaFor1000 = 200;
  SetQuotaSettings(1000, kPerStorageKeyQuotaFor1000,
                   kMustRemainAvailableForSystem);
  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 10 + 50 + 4000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 4000);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor1000);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor1000);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  auto client_result = GetUsageAndQuotaForStorageClient(
      ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(client_result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(client_result.usage, 0);
  EXPECT_EQ(client_result.quota, QuotaManagerImpl::kNoLimit);

  // Test when overbudgeted.
  const int kPerStorageKeyQuotaFor100 = 20;
  SetQuotaSettings(100, kPerStorageKeyQuotaFor100,
                   kMustRemainAvailableForSystem);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, storage_capacity.available_space + result.usage);

  client_result = GetUsageAndQuotaForStorageClient(
      ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(client_result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(client_result.usage, 0);
  EXPECT_EQ(client_result.quota, QuotaManagerImpl::kNoLimit);

  // Revoke the unlimited rights and make sure the change is noticed.
  mock_special_storage_policy()->Reset();
  mock_special_storage_policy()->NotifyCleared();

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 10 + 50 + 4000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage10/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://usage50/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 50);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor100);

  result = GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 4000);
  EXPECT_EQ(result.quota, kPerStorageKeyQuotaFor100);

  client_result = GetUsageAndQuotaForStorageClient(
      ToStorageKey("http://unlimited/"), kTemp);
  EXPECT_EQ(client_result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(client_result.usage, 4000);
  EXPECT_EQ(client_result.quota, kPerStorageKeyQuotaFor100);
}

TEST_F(QuotaManagerImplTest, GetQuotaLowAvailableDiskSpace) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 100000},
      {"http://unlimited/", kDefaultBucketName, kTemp, 4000000},
  };

  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  const int kPoolSize = 10000000;
  const int kPerStorageKeyQuota = kPoolSize / 5;

  // In here, we expect the low available space logic branch
  // to be ignored. Doing so should have QuotaManagerImpl return the same
  // per-host quota as what is set in QuotaSettings, despite being in a state of
  // low available space.
  const int kMustRemainAvailable =
      static_cast<int>(GetAvailableDiskSpaceForTest() - 65536);
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota, kMustRemainAvailable);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 100000);
  EXPECT_EQ(result.quota, kPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, GetSyncableQuota) {
  CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});

  // Pre-condition check: available disk space (for testing) is less than
  // the default quota for syncable storage.
  EXPECT_LE(kAvailableSpaceForApp,
            QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota);

  // The quota manager should return
  // QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota as syncable quota,
  // despite available space being less than the desired quota. Only
  // storage keys with unlimited storage, which is never the case for syncable
  // storage, shall have their quota calculation take into account the amount of
  // available disk space.
  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://unlimited/"), kSync);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota,
            QuotaManagerImpl::kSyncableStorageDefaultStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, GetUsage_Simple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kSync, 1},
      {"http://foo.com:1/", kDefaultBucketName, kSync, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 300},
      {"https://buz.com/", kDefaultBucketName, kTemp, 4000},
      {"http://buz.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com:1/", kDefaultBucketName, kSync, 600000},
      {"http://foo.com/", kDefaultBucketName, kTemp, 7000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  auto global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(300 + 4000 + 50000 + 7000000, global_usage_result.usage);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  EXPECT_EQ(
      1, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
             .usage);
  EXPECT_EQ(20, GetStorageKeyUsageWithBreakdown(
                    ToStorageKey("http://foo.com:1/"), kSync)
                    .usage);
  EXPECT_EQ(4000, GetStorageKeyUsageWithBreakdown(
                      ToStorageKey("https://buz.com/"), kTemp)
                      .usage);
  EXPECT_EQ(50000, GetStorageKeyUsageWithBreakdown(
                       ToStorageKey("http://buz.com/"), kTemp)
                       .usage);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithModification) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kSync, 1},
      {"http://foo.com:1/", kDefaultBucketName, kSync, 20},
      {"http://bar.com/", kDefaultBucketName, kTemp, 300},
      {"https://buz.com/", kDefaultBucketName, kTemp, 4000},
      {"http://buz.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com:1/", kDefaultBucketName, kSync, 600000},
      {"http://foo.com/", kDefaultBucketName, kTemp, 7000000},
  };

  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://foo.com/"), kSync,
                               80000000);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 1 + 20 + 600000 + 80000000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 300 + 4000 + 50000 + 7000000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://foo.com/"), kTemp,
                               1);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 300 + 4000 + 50000 + 7000000 + 1);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  EXPECT_EQ(4000, GetStorageKeyUsageWithBreakdown(
                      ToStorageKey("https://buz.com/"), kTemp)
                      .usage);
  EXPECT_EQ(50000, GetStorageKeyUsageWithBreakdown(
                       ToStorageKey("http://buz.com/"), kTemp)
                       .usage);

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://buz.com/"), kTemp,
                               900000000);

  EXPECT_EQ(4000, GetStorageKeyUsageWithBreakdown(
                      ToStorageKey("https://buz.com/"), kTemp)
                      .usage);
  EXPECT_EQ(50000 + 900000000, GetStorageKeyUsageWithBreakdown(
                                   ToStorageKey("http://buz.com/"), kTemp)
                                   .usage);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithBucketModification) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com/", kDefaultBucketName, kSync, 50},
      {"http://bar.com/", "logs", kTemp, 100},
  };

  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 50);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  ASSERT_OK_AND_ASSIGN(
      auto foo_temp_bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp));
  client->ModifyBucketAndNotify(foo_temp_bucket.ToBucketLocator(), 80000000);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 1 + 100 + 80000000);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 50);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  ASSERT_OK_AND_ASSIGN(
      auto foo_sync_bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kSync));
  client->ModifyBucketAndNotify(foo_sync_bucket.ToBucketLocator(), 200);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 50 + 200);
  EXPECT_EQ(global_usage_result.unlimited_usage, 0);

  EXPECT_EQ(100, GetStorageKeyUsageWithBreakdown(
                     ToStorageKey("http://bar.com/"), kTemp)
                     .usage);

  ASSERT_OK_AND_ASSIGN(
      auto bar_temp_bucket,
      GetBucket(ToStorageKey("http://bar.com/"), "logs", kTemp));
  client->ModifyBucketAndNotify(bar_temp_bucket.ToBucketLocator(), 900000000);

  EXPECT_EQ(100 + 900000000, GetStorageKeyUsageWithBreakdown(
                                 ToStorageKey("http://bar.com/"), kTemp)
                                 .usage);
}

TEST_F(QuotaManagerImplTest, GetUsage_WithDeleteBucket) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com/", "secondbucket", kTemp, 10000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_storage_key_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_storage_key_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;

  ASSERT_OK_AND_ASSIGN(auto bucket, GetBucket(ToStorageKey("http://foo.com/"),
                                              kDefaultBucketName, kTemp));

  auto status = DeleteBucketData(bucket.ToBucketLocator(),
                                 {QuotaClientType::kFileSystem});
  EXPECT_EQ(status, QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp - 1);

  EXPECT_EQ(
      predelete_storage_key_tmp - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_storage_key_sync,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);
}

TEST_F(QuotaManagerImplTest, GetStorageCapacity) {
  auto storage_capacity = GetStorageCapacity();
  EXPECT_GE(storage_capacity.total_space, 0);
  EXPECT_GE(storage_capacity.available_space, 0);
}

TEST_F(QuotaManagerImplTest, EvictBucketData) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", "logs", kTemp, 800000},
      {"http://foo.com/", "logs", kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", "logs", kTemp, 6000},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_storage_key_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_storage_key_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;

  for (const ClientBucketData& data : kData1) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  // Default bucket eviction.
  ASSERT_OK_AND_ASSIGN(auto bucket, GetBucket(ToStorageKey("http://foo.com/"),
                                              kDefaultBucketName, kTemp));

  EvictBucketData(bucket.ToBucketLocator());

  ASSERT_THAT(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp),
      base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - (1 + 50000), global_usage_result.usage);

  EXPECT_EQ(
      predelete_storage_key_tmp - (1 + 50000),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_storage_key_sync,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);

  // Non default bucket eviction.
  ASSERT_OK_AND_ASSIGN(
      bucket, GetBucket(ToStorageKey("http://foo.com"), "logs", kTemp));

  EvictBucketData(bucket.ToBucketLocator());

  EXPECT_THAT(GetBucket(ToStorageKey("http://foo.com"), "logs", kTemp),
              base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - (1 + 20 + 50000), global_usage_result.usage);

  EXPECT_EQ(
      predelete_storage_key_tmp - (1 + 20 + 50000),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_storage_key_sync,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);
}

TEST_F(QuotaManagerImplTest, EvictBucketDataHistogram) {
  base::HistogramTester histograms;
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(client, kData);

  GetGlobalUsage(kTemp);

  ASSERT_OK_AND_ASSIGN(auto bucket, GetBucket(ToStorageKey("http://foo.com"),
                                              kDefaultBucketName, kTemp));

  EvictBucketData(bucket.ToBucketLocator());

  // Ensure use count and time since access are recorded.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 1);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 0, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram, 1);

  // Change the use count.
  NotifyDefaultBucketAccessed(ToStorageKey("http://bar.com/"), kTemp,
                              base::Time::Now());
  task_environment_.RunUntilIdle();

  GetGlobalUsage(kTemp);

  ASSERT_OK_AND_ASSIGN(bucket, GetBucket(ToStorageKey("http://bar.com"),
                                         kDefaultBucketName, kTemp));

  EvictBucketData(bucket.ToBucketLocator());

  // The new use count should be logged.
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 2);
  histograms.ExpectBucketCount(
      QuotaManagerImpl::kEvictedBucketAccessedCountHistogram, 1, 1);
  histograms.ExpectTotalCount(
      QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram, 2);
}

TEST_F(QuotaManagerImplTest, EvictBucketDataWithDeletionError) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const int kNumberOfTemporaryBuckets = 3;
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, (1 + 20 + 4000));

  EXPECT_EQ(
      1, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);
  EXPECT_EQ(20, GetStorageKeyUsageWithBreakdown(
                    ToStorageKey("http://foo.com:1/"), kTemp)
                    .usage);
  EXPECT_EQ(300, GetStorageKeyUsageWithBreakdown(
                     ToStorageKey("http://foo.com/"), kSync)
                     .usage);

  for (const ClientBucketData& data : kData) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type);
  }
  task_environment_.RunUntilIdle();

  ASSERT_OK_AND_ASSIGN(auto bucket, GetBucket(ToStorageKey("http://foo.com/"),
                                              kDefaultBucketName, kTemp));
  client->AddBucketToErrorSet(bucket.ToBucketLocator());

  for (int i = 0; i < QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted + 1;
       ++i) {
    EvictBucketData(bucket.ToBucketLocator());
  }

  // The default bucket for "http://foo.com/" should still be in the database.
  EXPECT_TRUE(
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp)
          .has_value());

  for (size_t i = 0; i < kNumberOfTemporaryBuckets - 1; ++i) {
    GetEvictionBucket();
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(eviction_bucket().has_value());
    // "http://foo.com/" should not be in the LRU list.
    EXPECT_NE(std::string("http://foo.com/"),
              eviction_bucket()->storage_key.origin().GetURL().spec());
    DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  }

  // Now the LRU list must be empty.
  GetEvictionBucket();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(eviction_bucket().has_value());

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 1);

  EXPECT_EQ(
      1, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);
  EXPECT_EQ(0, GetStorageKeyUsageWithBreakdown(
                   ToStorageKey("http://foo.com:1/"), kTemp)
                   .usage);
  EXPECT_EQ(300, GetStorageKeyUsageWithBreakdown(
                     ToStorageKey("http://foo.com/"), kSync)
                     .usage);
}

TEST_F(QuotaManagerImplTest, GetEvictionRoundInfo) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://unlimited/", kDefaultBucketName, kTemp, 4000},
  };

  mock_special_storage_policy()->AddUnlimited(GURL("http://unlimited/"));
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  const int kPoolSize = 10000000;
  const int kPerStorageKeyQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota,
                   kMustRemainAvailableForSystem);

  GetEvictionRoundInfo();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(QuotaStatusCode::kOk, status());
  EXPECT_EQ(21, usage());
  EXPECT_EQ(kPoolSize, settings().pool_size);
  EXPECT_LE(0, available_space());
}

TEST_F(QuotaManagerImplTest, DeleteHostDataNoClients) {
  EXPECT_EQ(DeleteHostData(std::string(), kTemp), QuotaStatusCode::kOk);
}

TEST_F(QuotaManagerImplTest, DeleteHostDataSimple) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_storage_key_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_storage_key_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;

  EXPECT_EQ(DeleteHostData(std::string(), kTemp), QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  EXPECT_EQ(
      predelete_storage_key_tmp,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_storage_key_sync,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);

  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(predelete_global_tmp - 1, global_usage_result.usage);

  EXPECT_EQ(
      predelete_storage_key_tmp - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_storage_key_sync,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultiple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 6000},
      {"http://foo.com/", kDefaultBucketName, kSync, 700},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_sfoo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("https://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo1_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com:1/"), kTemp)
          .usage;
  const int64_t predelete_sk_bar_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;
  const int64_t predelete_sk_bar_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kSync)
          .usage;

  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("bar.com", kTemp), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("foo.com", kTemp), QuotaStatusCode::kOk);

  const BucketTableEntries& entries = DumpBucketTable();
  for (const auto& entry : entries) {
    if (entry->type != kStorageTemp) {
      continue;
    }

    std::optional<StorageKey> storage_key =
        StorageKey::Deserialize(entry->storage_key);
    ASSERT_TRUE(storage_key.has_value());

    EXPECT_NE(std::string("http://foo.com/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("http://foo.com:1/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("https://foo.com/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              std::move(storage_key).value().origin().GetURL().spec());
  }

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage,
            predelete_global_tmp - (1 + 20 + 4000 + 50000 + 6000 + 80 + 9));

  EXPECT_EQ(
      predelete_sk_foo_tmp - (1 + 50000),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_sk_sfoo_tmp - (80),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("https://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_sk_foo1_tmp - (20 + 6000),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com:1/"), kTemp)
          .usage);
  EXPECT_EQ(
      predelete_sk_bar_tmp - (4000 + 9),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage);
  EXPECT_EQ(predelete_sk_foo_sync, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://foo.com/"), kSync)
                                       .usage);
  EXPECT_EQ(predelete_sk_bar_sync, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://bar.com/"), kSync)
                                       .usage);
}

TEST_F(QuotaManagerImplTest, DeleteHostDataMultipleClientsDifferentTypes) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kSync, 1},
      {"http://foo.com:1/", kDefaultBucketName, kSync, 10},
      {"http://foo.com/", kDefaultBucketName, kTemp, 100},
      {"http://bar.com/", kDefaultBucketName, kSync, 1000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 100000},
      {"https://foo.com/", kDefaultBucketName, kTemp, 1000000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 10000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo1_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com:1/"), kTemp)
          .usage;
  const int64_t predelete_sk_bar_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage;

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, (1000 + 10 + 1));

  EXPECT_EQ(
      1, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
             .usage);
  EXPECT_EQ(10, GetStorageKeyUsageWithBreakdown(
                    ToStorageKey("http://foo.com:1/"), kSync)
                    .usage);
  EXPECT_EQ(1000, GetStorageKeyUsageWithBreakdown(
                      ToStorageKey("http://bar.com/"), kSync)
                      .usage);

  EXPECT_EQ(DeleteHostData("foo.com", kSync), QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteHostData("bar.com", kSync), QuotaStatusCode::kOk);

  const BucketTableEntries& entries = DumpBucketTable();
  for (const auto& entry : entries) {
    if (entry->type != kStorageSync) {
      continue;
    }

    std::optional<StorageKey> storage_key =
        StorageKey::Deserialize(entry->storage_key);
    ASSERT_TRUE(storage_key.has_value());

    EXPECT_NE(std::string("http://foo.com/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("http://foo.com:1/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("https://foo.com/"),
              storage_key.value().origin().GetURL().spec());
    EXPECT_NE(std::string("http://bar.com/"),
              std::move(storage_key).value().origin().GetURL().spec());
  }

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  EXPECT_EQ(predelete_sk_foo_tmp, GetStorageKeyUsageWithBreakdown(
                                      ToStorageKey("http://foo.com/"), kTemp)
                                      .usage);
  EXPECT_EQ(predelete_sk_foo1_tmp, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://foo.com:1/"), kTemp)
                                       .usage);
  EXPECT_EQ(predelete_sk_bar_tmp, GetStorageKeyUsageWithBreakdown(
                                      ToStorageKey("http://bar.com/"), kTemp)
                                      .usage);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, 0);

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
             .usage);
  EXPECT_EQ(0, GetStorageKeyUsageWithBreakdown(
                   ToStorageKey("http://foo.com:1/"), kSync)
                   .usage);
  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kSync)
             .usage);
}

TEST_F(QuotaManagerImplTest, DeleteBucketNoClients) {
  ASSERT_OK_AND_ASSIGN(auto bucket,
                       CreateBucketForTesting(ToStorageKey("http://foo.com"),
                                              kDefaultBucketName, kTemp));
  EXPECT_EQ(DeleteBucketData(bucket.ToBucketLocator(), AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
}

TEST_F(QuotaManagerImplTest, DeleteBucketDataMultiple) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 20},
      {"http://foo.com/", kDefaultBucketName, kSync, 300},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 6000},
      {"http://foo.com/", kDefaultBucketName, kSync, 700},
      {"https://foo.com/", kDefaultBucketName, kTemp, 80},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  ASSERT_OK_AND_ASSIGN(
      auto foo_temp_bucket,
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp));

  ASSERT_OK_AND_ASSIGN(
      auto bar_temp_bucket,
      GetBucket(ToStorageKey("http://bar.com"), kDefaultBucketName, kTemp));

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_sfoo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("https://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo1_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com:1/"), kTemp)
          .usage;
  const int64_t predelete_sk_bar_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;
  const int64_t predelete_sk_bar_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kSync)
          .usage;

  for (const ClientBucketData& data : kData1) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DeleteBucketData(foo_temp_bucket.ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteBucketData(bar_temp_bucket.ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);

  EXPECT_THAT(GetBucket(foo_temp_bucket.storage_key, foo_temp_bucket.name,
                        foo_temp_bucket.type),
              base::test::ErrorIs(QuotaError::kNotFound));

  EXPECT_THAT(GetBucket(bar_temp_bucket.storage_key, bar_temp_bucket.name,
                        bar_temp_bucket.type),
              base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage,
            predelete_global_tmp - (1 + 4000 + 50000 + 9));

  EXPECT_EQ(
      predelete_sk_foo_tmp - (1 + 50000),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
  EXPECT_EQ(predelete_sk_sfoo_tmp, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("https://foo.com/"), kTemp)
                                       .usage);
  EXPECT_EQ(predelete_sk_foo1_tmp, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://foo.com:1/"), kTemp)
                                       .usage);
  EXPECT_EQ(
      predelete_sk_bar_tmp - (4000 + 9),
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage);
  EXPECT_EQ(predelete_sk_foo_sync, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://foo.com/"), kSync)
                                       .usage);
  EXPECT_EQ(predelete_sk_bar_sync, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://bar.com/"), kSync)
                                       .usage);
}

TEST_F(QuotaManagerImplTest, DeleteBucketDataMultipleClientsDifferentTypes) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kSync, 1},
      {"http://foo.com:1/", kDefaultBucketName, kSync, 10},
      {"http://foo.com/", kDefaultBucketName, kTemp, 100},
      {"http://bar.com/", kDefaultBucketName, kSync, 1000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10000},
      {"http://foo.com:1/", kDefaultBucketName, kTemp, 100000},
      {"https://foo.com/", kDefaultBucketName, kTemp, 1000000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 10000000},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  ASSERT_OK_AND_ASSIGN(
      auto foo_sync_bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kSync));

  ASSERT_OK_AND_ASSIGN(
      auto bar_sync_bucket,
      GetBucket(ToStorageKey("http://bar.com/"), kDefaultBucketName, kSync));

  auto global_usage_result = GetGlobalUsage(kTemp);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  global_usage_result = GetGlobalUsage(kSync);
  const int64_t predelete_global_sync = global_usage_result.usage;

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_sfoo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("https://foo.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo1_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com:1/"), kTemp)
          .usage;
  const int64_t predelete_sk_bar_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
          .usage;
  const int64_t predelete_sk_foo_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage;
  const int64_t predelete_sk_bar_sync =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kSync)
          .usage;

  for (const ClientBucketData& data : kData1) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  for (const ClientBucketData& data : kData2) {
    NotifyDefaultBucketAccessed(ToStorageKey(data.origin), data.type,
                                base::Time::Now());
  }
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DeleteBucketData(foo_sync_bucket.ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);
  EXPECT_EQ(DeleteBucketData(bar_sync_bucket.ToBucketLocator(),
                             AllQuotaClientTypes()),
            QuotaStatusCode::kOk);

  EXPECT_THAT(GetBucket(foo_sync_bucket.storage_key, foo_sync_bucket.name,
                        foo_sync_bucket.type),
              base::test::ErrorIs(QuotaError::kNotFound));

  EXPECT_THAT(GetBucket(bar_sync_bucket.storage_key, bar_sync_bucket.name,
                        bar_sync_bucket.type),
              base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp);

  EXPECT_EQ(predelete_sk_foo_tmp, GetStorageKeyUsageWithBreakdown(
                                      ToStorageKey("http://foo.com/"), kTemp)
                                      .usage);
  EXPECT_EQ(predelete_sk_sfoo_tmp, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("https://foo.com/"), kTemp)
                                       .usage);
  EXPECT_EQ(predelete_sk_foo1_tmp, GetStorageKeyUsageWithBreakdown(
                                       ToStorageKey("http://foo.com:1/"), kTemp)
                                       .usage);
  EXPECT_EQ(predelete_sk_bar_tmp, GetStorageKeyUsageWithBreakdown(
                                      ToStorageKey("http://bar.com/"), kTemp)
                                      .usage);

  global_usage_result = GetGlobalUsage(kSync);
  EXPECT_EQ(global_usage_result.usage, predelete_global_sync - (1 + 1000));

  EXPECT_EQ(
      predelete_sk_foo_sync - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kSync)
          .usage);
  EXPECT_EQ(
      predelete_sk_bar_sync - 1000,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kSync)
          .usage);
}

TEST_F(QuotaManagerImplTest, FindAndDeleteBucketData) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
      {"http://bar.com/", kDefaultBucketName, kTemp, 4000},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 50000},
      {"http://bar.com/", kDefaultBucketName, kTemp, 9},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(db_client, kData2);

  ASSERT_OK_AND_ASSIGN(
      auto foo_bucket,
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp));

  ASSERT_OK_AND_ASSIGN(
      auto bar_bucket,
      GetBucket(ToStorageKey("http://bar.com"), kDefaultBucketName, kTemp));

  // Check usage data before deletion.
  auto global_usage_result = GetGlobalUsage(kTemp);
  ASSERT_EQ((1 + 9 + 4000 + 50000), global_usage_result.usage);
  const int64_t predelete_global_tmp = global_usage_result.usage;

  ASSERT_EQ((1 + 50000), GetStorageKeyUsageWithBreakdown(
                             ToStorageKey("http://foo.com/"), kTemp)
                             .usage);
  ASSERT_EQ((9 + 4000), GetStorageKeyUsageWithBreakdown(
                            ToStorageKey("http://bar.com/"), kTemp)
                            .usage);

  // Delete bucket for "http://foo.com/".
  EXPECT_EQ(FindAndDeleteBucketData(foo_bucket.storage_key, foo_bucket.name),
            QuotaStatusCode::kOk);

  EXPECT_THAT(
      GetBucket(foo_bucket.storage_key, foo_bucket.name, foo_bucket.type),
      base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, predelete_global_tmp - (1 + 50000));

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);

  // Delete bucket for "http://bar.com/".
  EXPECT_EQ(FindAndDeleteBucketData(bar_bucket.storage_key, bar_bucket.name),
            QuotaStatusCode::kOk);

  EXPECT_THAT(
      GetBucket(bar_bucket.storage_key, bar_bucket.name, bar_bucket.type),
      base::test::ErrorIs(QuotaError::kNotFound));

  global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://bar.com/"), kTemp)
             .usage);
}

TEST_F(QuotaManagerImplTest, FindAndDeleteBucketDataWithDBError) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 123},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});

  RegisterClientBucketData(fs_client, kData);

  // Check usage data before deletion.
  ASSERT_EQ(123, GetStorageKeyUsageWithBreakdown(
                     ToStorageKey("http://foo.com/"), kTemp)
                     .usage);

  // Bucket lookup uses the `buckets_by_storage_key` index. So, we can corrupt
  // any other index, and SQLite will only detect the corruption when trying to
  // delete a bucket.
  QuotaError corruption_error = CorruptDatabaseForTesting(
      base::BindOnce([](const base::FilePath& db_path) {
        ASSERT_TRUE(sql::test::CorruptIndexRootPage(
            db_path, "buckets_by_last_accessed"));
      }));
  ASSERT_EQ(QuotaError::kNone, corruption_error);

  // Deleting the bucket will result in an error.
  EXPECT_NE(FindAndDeleteBucketData(ToStorageKey("http://foo.com"),
                                    kDefaultBucketName),
            QuotaStatusCode::kOk);

  auto global_usage_result = GetGlobalUsage(kTemp);
  EXPECT_EQ(global_usage_result.usage, 0);

  EXPECT_EQ(
      0, GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
             .usage);
}

TEST_F(QuotaManagerImplTest, GetDiskAvailabilityAndTempPoolSize) {
  ResetQuotaManagerImpl(/*is_incognito=*/false);

  base::test::TestFuture<int64_t, int64_t, int64_t> quota_internals_future;
  quota_manager_impl()->GetDiskAvailabilityAndTempPoolSize(
      quota_internals_future.GetCallback());
  std::tuple quota_internals_result = quota_internals_future.Take();

  int64_t available_space =
      static_cast<uint64_t>(GetAvailableDiskSpaceForTest());
  int64_t total_space = available_space * 2;

  EXPECT_EQ(total_space, std::get<0>(quota_internals_result));
  EXPECT_EQ(available_space, std::get<1>(quota_internals_result));
  EXPECT_EQ(kDefaultPoolSize, std::get<2>(quota_internals_result));
}

TEST_F(QuotaManagerImplTest, GetDiskAvailabilityAndTempPoolSize_Incognito) {
  // Test to make sure total_space and available_space are retrieved
  // as expected, without producing a crash.
  ResetQuotaManagerImpl(/*is_incognito=*/true);

  base::test::TestFuture<int64_t, int64_t, int64_t> quota_internals_future;
  quota_manager_impl()->GetDiskAvailabilityAndTempPoolSize(
      quota_internals_future.GetCallback());
  std::tuple quota_internals_result = quota_internals_future.Take();

  EXPECT_EQ(kDefaultPoolSize, std::get<0>(quota_internals_result));
  EXPECT_EQ(kDefaultPoolSize, std::get<1>(quota_internals_result));
  EXPECT_EQ(kDefaultPoolSize, std::get<2>(quota_internals_result));
}

TEST_F(QuotaManagerImplTest, NotifyAndLRUBucket) {
  static const ClientBucketData kData[] = {
      {"http://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://a.com:1/", kDefaultBucketName, kTemp, 0},
      {"http://b.com/", kDefaultBucketName, kSync, 0},
      {"http://c.com/", kDefaultBucketName, kTemp, 0},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(fs_client, kData);

  NotifyDefaultBucketAccessed(ToStorageKey("http://b.com/"), kSync,
                              base::Time::Now());
  NotifyDefaultBucketAccessed(ToStorageKey("http://a.com/"), kTemp,
                              base::Time::Now());
  NotifyDefaultBucketAccessed(ToStorageKey("http://c.com/"), kTemp,
                              base::Time::Now());
  task_environment_.RunUntilIdle();

  GetEvictionBucket();
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com:1/",
            eviction_bucket()->storage_key.origin().GetURL().spec());

  DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  GetEvictionBucket();
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://a.com/",
            eviction_bucket()->storage_key.origin().GetURL().spec());

  DeleteBucketData(*eviction_bucket(), AllQuotaClientTypes());
  GetEvictionBucket();
  task_environment_.RunUntilIdle();
  EXPECT_EQ("http://c.com/",
            eviction_bucket()->storage_key.origin().GetURL().spec());
}

TEST_F(QuotaManagerImplTest, GetBucketsForEviction) {
  static const ClientBucketData kData[] = {
      {"http://a.com/", kDefaultBucketName, kTemp, 107},
      {"http://b.com/", kDefaultBucketName, kTemp, 300},
      {"http://c.com/", kDefaultBucketName, kTemp, 713},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(client, kData);
  GetGlobalUsage(kTemp);

  NotifyDefaultBucketAccessed(ToStorageKey("http://a.com/"), kTemp,
                              base::Time::Now());
  NotifyDefaultBucketAccessed(ToStorageKey("http://b.com/"), kTemp,
                              base::Time::Now());
  NotifyDefaultBucketAccessed(ToStorageKey("http://c.com/"), kTemp,
                              base::Time::Now());

  auto buckets = GetEvictionBuckets(110);
  EXPECT_THAT(buckets, testing::UnorderedElementsAre(
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://a.com")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://b.com"))));

  // Notify that the `bucket_a` is accessed. Now b is the LRU (and also happens
  // to satisfy the desire to evict 110b of data).
  NotifyDefaultBucketAccessed(ToStorageKey("http://a.com/"), kTemp,
                              base::Time::Now());
  buckets = GetEvictionBuckets(110);
  EXPECT_THAT(buckets,
              testing::UnorderedElementsAre(testing::Field(
                  &BucketLocator::storage_key, ToStorageKey("http://b.com"))));
}

TEST_F(QuotaManagerImplTest, GetBucketsModifiedBetween) {
  static const ClientBucketData kData[] = {
      {"http://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://a.com:1/", kDefaultBucketName, kTemp, 0},
      {"https://a.com/", kDefaultBucketName, kTemp, 0},
      {"http://b.com/", kDefaultBucketName, kSync, 0},
      {"http://c.com/", kDefaultBucketName, kTemp, 0},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  auto buckets =
      GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());
  EXPECT_EQ(4U, buckets.size());

  base::Time time1 = client->IncrementMockTime();
  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://a.com/"), kTemp,
                               10);
  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://a.com:1/"), kTemp,
                               10);
  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://b.com/"), kSync,
                               10);
  base::Time time2 = client->IncrementMockTime();
  ModifyDefaultBucketAndNotify(client, ToStorageKey("https://a.com/"), kTemp,
                               10);
  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://c.com/"), kTemp,
                               10);
  base::Time time3 = client->IncrementMockTime();

  // Database call to ensure modification calls have completed.
  std::ignore =
      GetBucket(ToStorageKey("http://a.com"), kDefaultBucketName, kTemp);

  buckets = GetBucketsModifiedBetween(kTemp, time1, base::Time::Max());
  EXPECT_THAT(buckets, testing::UnorderedElementsAre(
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://a.com")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://a.com:1")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("https://a.com")),
                           testing::Field(&BucketLocator::storage_key,
                                          ToStorageKey("http://c.com"))));

  buckets = GetBucketsModifiedBetween(kTemp, time2, base::Time::Max());
  EXPECT_EQ(2U, buckets.size());

  buckets = GetBucketsModifiedBetween(kTemp, time3, base::Time::Max());
  EXPECT_TRUE(buckets.empty());

  ModifyDefaultBucketAndNotify(client, ToStorageKey("http://a.com/"), kTemp,
                               10);

  // Database call to ensure modification calls have completed.
  std::ignore =
      GetBucket(ToStorageKey("http://a.com"), kDefaultBucketName, kTemp);

  buckets = GetBucketsModifiedBetween(kTemp, time3, base::Time::Max());
  EXPECT_THAT(buckets,
              testing::UnorderedElementsAre(testing::Field(
                  &BucketLocator::storage_key, ToStorageKey("http://a.com/"))));
}

TEST_F(QuotaManagerImplTest, GetBucketsModifiedBetweenWithDatabaseError) {
  disable_database_bootstrap(true);
  OpenDatabase();

  // Disable quota database for database error behavior.
  DisableQuotaDatabase();

  auto buckets =
      GetBucketsModifiedBetween(kTemp, base::Time(), base::Time::Max());

  // Return empty set when error is encountered.
  EXPECT_TRUE(buckets.empty());
}

TEST_F(QuotaManagerImplTest, DumpBucketTable) {
  // Dumping an unpopulated bucket table returns an empty vector.
  const BucketTableEntries& initial_entries = DumpBucketTable();
  EXPECT_TRUE(initial_entries.empty());

  const StorageKey kStorageKey = ToStorageKey("http://example.com/");
  std::ignore = CreateBucketForTesting(kStorageKey, kDefaultBucketName, kTemp);
  std::ignore = CreateBucketForTesting(kStorageKey, kDefaultBucketName, kSync);

  NotifyDefaultBucketAccessed(kStorageKey, kTemp, base::Time::Now());
  NotifyDefaultBucketAccessed(kStorageKey, kSync, base::Time::Now());
  NotifyDefaultBucketAccessed(kStorageKey, kSync, base::Time::Now());
  task_environment_.RunUntilIdle();

  const BucketTableEntries& entries = DumpBucketTable();
  EXPECT_THAT(
      entries,
      testing::UnorderedElementsAre(
          MatchesBucketTableEntry(kStorageKey.Serialize(), kStorageTemp, 1),
          MatchesBucketTableEntry(kStorageKey.Serialize(), kStorageSync, 2)));
}

TEST_F(QuotaManagerImplTest, RetrieveBucketsTable) {
  const StorageKey kStorageKey = ToStorageKey("http://example.com/");
  const std::string kSerializedStorageKey = kStorageKey.Serialize();
  const base::Time kAccessTime = base::Time::Now();

  static const ClientBucketData kData[] = {
      {"http://example.com/", kDefaultBucketName, kTemp, 123},
      {"http://example.com/", kDefaultBucketName, kSync, 456},
  };

  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp, kSync});
  RegisterClientBucketData(client, kData);

  NotifyDefaultBucketAccessed(kStorageKey, kTemp, kAccessTime);
  NotifyDefaultBucketAccessed(kStorageKey, kSync, kAccessTime);
  const base::Time time1 = base::Time::Now();

  auto temp_bucket = GetBucket(kStorageKey, kDefaultBucketName, kTemp);
  auto sync_bucket = GetBucket(kStorageKey, kDefaultBucketName, kSync);

  const std::vector<storage::mojom::BucketTableEntryPtr> bucket_table_entries =
      RetrieveBucketsTable();

  auto* temp_entry =
      FindBucketTableEntry(bucket_table_entries, temp_bucket->id);
  EXPECT_TRUE(temp_entry);
  EXPECT_EQ(temp_entry->storage_key, kSerializedStorageKey);
  EXPECT_EQ(temp_entry->type, kStorageTemp);
  EXPECT_EQ(temp_entry->name, kDefaultBucketName);
  EXPECT_EQ(temp_entry->use_count, 1);
  EXPECT_EQ(temp_entry->last_accessed, kAccessTime);
  EXPECT_GE(temp_entry->last_modified, kAccessTime);
  EXPECT_LE(temp_entry->last_modified, time1);
  EXPECT_EQ(temp_entry->usage, 123);

  auto* sync_entry =
      FindBucketTableEntry(bucket_table_entries, sync_bucket->id);
  EXPECT_TRUE(sync_entry);
  EXPECT_EQ(sync_entry->storage_key, kSerializedStorageKey);
  EXPECT_EQ(sync_entry->type, kStorageSync);
  EXPECT_EQ(sync_entry->name, kDefaultBucketName);
  EXPECT_EQ(sync_entry->use_count, 1);
  EXPECT_EQ(sync_entry->last_accessed, kAccessTime);
  EXPECT_GE(temp_entry->last_modified, kAccessTime);
  EXPECT_LE(temp_entry->last_modified, time1);
  EXPECT_EQ(sync_entry->usage, 456);
}

TEST_F(QuotaManagerImplTest, DeleteSpecificClientTypeSingleBucket) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 2},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData4[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* idb_client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(sw_client, kData2);
  RegisterClientBucketData(db_client, kData3);
  RegisterClientBucketData(idb_client, kData4);

  ASSERT_OK_AND_ASSIGN(
      auto foo_bucket,
      GetBucket(ToStorageKey("http://foo.com"), kDefaultBucketName, kTemp));

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;

  DeleteBucketData(foo_bucket.ToBucketLocator(),
                   {QuotaClientType::kFileSystem});
  EXPECT_EQ(
      predelete_sk_foo_tmp - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);

  DeleteBucketData(foo_bucket.ToBucketLocator(),
                   {QuotaClientType::kServiceWorkerCache});
  EXPECT_EQ(
      predelete_sk_foo_tmp - 2 - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);

  DeleteBucketData(foo_bucket.ToBucketLocator(), {QuotaClientType::kDatabase});
  EXPECT_EQ(
      predelete_sk_foo_tmp - 4 - 2 - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);

  DeleteBucketData(foo_bucket.ToBucketLocator(),
                   {QuotaClientType::kIndexedDatabase});
  EXPECT_EQ(
      predelete_sk_foo_tmp - 8 - 4 - 2 - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
}

TEST_F(QuotaManagerImplTest, DeleteMultipleClientTypesSingleBucket) {
  static const ClientBucketData kData1[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  static const ClientBucketData kData2[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 2},
  };
  static const ClientBucketData kData3[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 4},
  };
  static const ClientBucketData kData4[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 8},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  MockQuotaClient* sw_client =
      CreateAndRegisterClient(QuotaClientType::kServiceWorkerCache, {kTemp});
  MockQuotaClient* db_client =
      CreateAndRegisterClient(QuotaClientType::kDatabase, {kTemp});
  MockQuotaClient* idb_client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(fs_client, kData1);
  RegisterClientBucketData(sw_client, kData2);
  RegisterClientBucketData(db_client, kData3);
  RegisterClientBucketData(idb_client, kData4);

  ASSERT_OK_AND_ASSIGN(
      auto foo_bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp));

  const int64_t predelete_sk_foo_tmp =
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage;

  DeleteBucketData(foo_bucket.ToBucketLocator(),
                   {QuotaClientType::kFileSystem, QuotaClientType::kDatabase});

  EXPECT_EQ(
      predelete_sk_foo_tmp - 4 - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);

  DeleteBucketData(foo_bucket.ToBucketLocator(),
                   {QuotaClientType::kServiceWorkerCache,
                    QuotaClientType::kIndexedDatabase});

  EXPECT_EQ(
      predelete_sk_foo_tmp - 8 - 4 - 2 - 1,
      GetStorageKeyUsageWithBreakdown(ToStorageKey("http://foo.com/"), kTemp)
          .usage);
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_Incognito) {
  ResetQuotaManagerImpl(true);

  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 10},
  };
  MockQuotaClient* fs_client =
      CreateAndRegisterClient(QuotaClientType::kFileSystem, {kTemp});
  RegisterClientBucketData(fs_client, kData);

  // Query global usage to warmup the usage tracker caching.
  GetGlobalUsage(kTemp);

  const int kPoolSize = 1000;
  const int kPerStorageKeyQuota = kPoolSize / 5;
  SetQuotaSettings(kPoolSize, kPerStorageKeyQuota, INT64_C(0));

  auto storage_capacity = GetStorageCapacity();
  EXPECT_EQ(storage_capacity.total_space, kPoolSize);
  EXPECT_EQ(storage_capacity.available_space, kPoolSize - 10);

  auto result =
      GetUsageAndQuotaForWebApps(ToStorageKey("http://foo.com/"), kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 10);
  EXPECT_GE(result.quota, kPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, GetUsageAndQuota_SessionOnly) {
  const StorageKey kEpheremalStorageKey = ToStorageKey("http://ephemeral/");
  mock_special_storage_policy()->AddSessionOnly(
      kEpheremalStorageKey.origin().GetURL());

  auto result = GetUsageAndQuotaForWebApps(kEpheremalStorageKey, kTemp);
  EXPECT_EQ(quota_manager_impl()->settings().session_only_per_storage_key_quota,
            result.quota);
}

TEST_F(QuotaManagerImplTest, MaybeRunStoragePressureCallback) {
  bool callback_ran = false;
  auto cb = base::BindRepeating(
      [](bool* callback_ran, const StorageKey& storage_key) {
        *callback_ran = true;
      },
      &callback_ran);

  SetStoragePressureCallback(cb);

  int64_t kGBytes = QuotaManagerImpl::kMBytes * 1024;
  MaybeRunStoragePressureCallback(StorageKey(), 100 * kGBytes, 2 * kGBytes);
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(callback_ran);

  MaybeRunStoragePressureCallback(StorageKey(), 100 * kGBytes, kGBytes);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_ran);
}

TEST_F(QuotaManagerImplTest, OverrideQuotaForStorageKey) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle = GetQuotaOverrideHandle();

  base::RunLoop run_loop;
  handle->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.usage, 0);
  EXPECT_EQ(result.quota, 5000);
}

TEST_F(QuotaManagerImplTest, OverrideQuotaForStorageKey_Disable) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle1 = GetQuotaOverrideHandle();
  std::unique_ptr<QuotaOverrideHandle> handle2 = GetQuotaOverrideHandle();

  base::RunLoop run_loop1;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop1.Quit(); }));
  run_loop1.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 5000);

  base::RunLoop run_loop2;
  handle2->OverrideQuotaForStorageKey(
      storage_key, 9000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 9000);

  base::RunLoop run_loop3;
  handle2->OverrideQuotaForStorageKey(
      storage_key, std::nullopt,
      base::BindLambdaForTesting([&]() { run_loop3.Quit(); }));
  run_loop3.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, kDefaultPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, WithdrawQuotaOverride) {
  StorageKey storage_key = ToStorageKey("https://foo.com");
  std::unique_ptr<QuotaOverrideHandle> handle1 = GetQuotaOverrideHandle();
  std::unique_ptr<QuotaOverrideHandle> handle2 = GetQuotaOverrideHandle();

  base::RunLoop run_loop1;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 5000,
      base::BindLambdaForTesting([&]() { run_loop1.Quit(); }));
  run_loop1.Run();

  auto result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 5000);

  base::RunLoop run_loop2;
  handle1->OverrideQuotaForStorageKey(
      storage_key, 8000,
      base::BindLambdaForTesting([&]() { run_loop2.Quit(); }));
  run_loop2.Run();

  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 8000);

  // Quota should remain overridden if only one of the two handles withdraws
  // it's overrides
  handle2.reset();
  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, 8000);

  handle1.reset();
  task_environment_.RunUntilIdle();
  result = GetUsageAndQuotaForWebApps(storage_key, kTemp);
  EXPECT_EQ(result.status, QuotaStatusCode::kOk);
  EXPECT_EQ(result.quota, kDefaultPerStorageKeyQuota);
}

TEST_F(QuotaManagerImplTest, QuotaChangeEvent_LargePartitionPressure) {
  scoped_feature_list_.InitAndEnableFeature(features::kStoragePressureEvent);
  bool quota_change_dispatched = false;

  SetQuotaChangeCallback(
      base::BindLambdaForTesting([&] { quota_change_dispatched = true; }));
  SetGetVolumeInfoFn([](const base::FilePath&) -> QuotaAvailability {
    int64_t total = kGigabytes * 100;
    int64_t available = kGigabytes * 2;
    return QuotaAvailability(total, available);
  });
  GetStorageCapacity();
  EXPECT_FALSE(quota_change_dispatched);

  SetGetVolumeInfoFn([](const base::FilePath&) -> QuotaAvailability {
    int64_t total = kGigabytes * 100;
    int64_t available = QuotaManagerImpl::kMBytes * 512;
    return QuotaAvailability(total, available);
  });
  GetStorageCapacity();
  EXPECT_TRUE(quota_change_dispatched);
}

TEST_F(QuotaManagerImplTest, QuotaChangeEvent_SmallPartitionPressure) {
  scoped_feature_list_.InitAndEnableFeature(features::kStoragePressureEvent);
  bool quota_change_dispatched = false;

  SetQuotaChangeCallback(
      base::BindLambdaForTesting([&] { quota_change_dispatched = true; }));
  SetGetVolumeInfoFn([](const base::FilePath&) -> QuotaAvailability {
    int64_t total = kGigabytes * 10;
    int64_t available = total * 2;
    return QuotaAvailability(total, available);
  });
  GetStorageCapacity();
  EXPECT_FALSE(quota_change_dispatched);

  SetGetVolumeInfoFn([](const base::FilePath&) -> QuotaAvailability {
    // DetermineStoragePressure flow will trigger the storage pressure flow
    // when available disk space is below 5% (+/- 0.25%) of total disk space.
    // Available is 2% here to guarantee that it falls below the threshold.
    int64_t total = kGigabytes * 10;
    int64_t available = total * 0.02;
    return QuotaAvailability(total, available);
  });
  GetStorageCapacity();
  EXPECT_TRUE(quota_change_dispatched);
}

TEST_F(QuotaManagerImplTest, DeleteBucketData_QuotaManagerDeletedImmediately) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp));

  base::test::TestFuture<QuotaStatusCode> delete_bucket_data_future;
  quota_manager_impl_->DeleteBucketData(
      bucket.ToBucketLocator(), {QuotaClientType::kIndexedDatabase},
      delete_bucket_data_future.GetCallback());
  quota_manager_impl_.reset();
  EXPECT_NE(QuotaStatusCode::kOk, delete_bucket_data_future.Get());
}

TEST_F(QuotaManagerImplTest, DeleteBucketData_CallbackDeletesQuotaManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp));

  base::RunLoop run_loop;
  QuotaStatusCode delete_bucket_data_result = QuotaStatusCode::kUnknown;
  quota_manager_impl_->DeleteBucketData(
      bucket.ToBucketLocator(), {QuotaClientType::kIndexedDatabase},
      base::BindLambdaForTesting([&](QuotaStatusCode status_code) {
        quota_manager_impl_.reset();
        delete_bucket_data_result = status_code;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(QuotaStatusCode::kOk, delete_bucket_data_result);
}

TEST_F(QuotaManagerImplTest, DeleteHostData_CallbackDeletesQuotaManager) {
  static const ClientBucketData kData[] = {
      {"http://foo.com/", kDefaultBucketName, kTemp, 1},
  };
  MockQuotaClient* client =
      CreateAndRegisterClient(QuotaClientType::kIndexedDatabase, {kTemp});
  RegisterClientBucketData(client, kData);

  ASSERT_OK_AND_ASSIGN(
      BucketInfo bucket,
      GetBucket(ToStorageKey("http://foo.com/"), kDefaultBucketName, kTemp));

  auto status = DeleteBucketData(bucket.ToBucketLocator(),
                                 {QuotaClientType::kFileSystem});
  EXPECT_EQ(status, QuotaStatusCode::kOk);

  base::RunLoop run_loop;
  QuotaStatusCode delete_host_data_result = QuotaStatusCode::kUnknown;
  quota_manager_impl_->DeleteHostData(
      "foo.com", kTemp,
      base::BindLambdaForTesting([&](QuotaStatusCode status_code) {
        quota_manager_impl_.reset();
        delete_host_data_result = status_code;
        run_loop.Quit();
      }));
  run_loop.Run();

  EXPECT_EQ(QuotaStatusCode::kOk, delete_host_data_result);
}

TEST_F(QuotaManagerImplTest, SimulateStoragePressure_Incognito) {
  bool callback_ran = false;

  auto cb = base::BindLambdaForTesting(
      [&callback_ran](const StorageKey& storage_key) { callback_ran = true; });

  SetStoragePressureCallback(cb);

  ResetQuotaManagerImpl(/*is_incognito=*/true);

  // This command should return and never execute the callback since it was
  // setup to be in Incognito.
  quota_manager_impl_->SimulateStoragePressure(
      url::Origin::Create(GURL("https://example.com")));

  EXPECT_FALSE(callback_ran);
}

TEST_F(QuotaManagerImplTest,
       QuotaManagerObserver_NotifiedOnAddedChangedAndDeleted) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  QuotaDatabase::SetClockForTesting(clock.get());
  clock->SetNow(base::Time::Now());

  SetupQuotaManagerObserver();

  BucketInitParams params(ToStorageKey("http://a.com/"), "bucket_a");

  // Create bucket.
  ASSERT_OK_AND_ASSIGN(auto bucket, UpdateOrCreateBucket(params));
  RunUntilObserverNotifies();

  ASSERT_EQ(observer_notifications_.size(), 1U);
  ObserverNotification notification = observer_notifications_[0];
  ASSERT_EQ(notification.type, kCreateOrUpdate);
  ASSERT_EQ(notification.bucket_info, bucket);
  observer_notifications_.clear();

  params.persistent = true;
  params.expiration = clock->Now() + base::Days(1);

  // Update bucket.
  ASSERT_OK_AND_ASSIGN(auto updated_bucket, UpdateOrCreateBucket(params));
  RunUntilObserverNotifies();

  ASSERT_EQ(observer_notifications_.size(), 1U);
  notification = observer_notifications_[0];
  ASSERT_EQ(notification.type, kCreateOrUpdate);
  EXPECT_EQ(notification.bucket_info, updated_bucket);
  EXPECT_EQ(notification.bucket_info->persistent, params.persistent);
  EXPECT_EQ(notification.bucket_info->expiration, params.expiration);
  observer_notifications_.clear();

  // Delete bucket.
  auto status =
      DeleteBucketData(bucket.ToBucketLocator(), AllQuotaClientTypes());
  RunUntilObserverNotifies();

  ASSERT_EQ(status, QuotaStatusCode::kOk);
  ASSERT_EQ(observer_notifications_.size(), 1U);
  notification = observer_notifications_[0];
  ASSERT_EQ(notification.type, kDelete);
  EXPECT_EQ(notification.bucket_locator, updated_bucket.ToBucketLocator());

  QuotaDatabase::SetClockForTesting(nullptr);
}

TEST_F(QuotaManagerImplTest, QuotaManagerObserver_NotifiedOnExpired) {
  auto clock = std::make_unique<base::SimpleTestClock>();
  QuotaDatabase::SetClockForTesting(clock.get());
  clock->SetNow(base::Time::Now());

  SetupQuotaManagerObserver();

  BucketInitParams params(ToStorageKey("http://a.com/"), "bucket_a");
  params.expiration = clock->Now() + base::Days(5);

  ASSERT_OK_AND_ASSIGN(auto bucket, UpdateOrCreateBucket(params));
  RunUntilObserverNotifies();

  ASSERT_EQ(observer_notifications_.size(), 1U);
  ObserverNotification notification = observer_notifications_[0];
  ASSERT_EQ(notification.type, kCreateOrUpdate);
  ASSERT_EQ(notification.bucket_info, bucket);
  observer_notifications_.clear();

  clock->Advance(base::Days(20));
  base::test::TestFuture<QuotaStatusCode> future;
  quota_manager_impl_->EvictExpiredBuckets(future.GetCallback());
  EXPECT_EQ(QuotaStatusCode::kOk, future.Get());

  EXPECT_FALSE(GetBucketById(bucket.id).has_value());
  ASSERT_EQ(observer_notifications_.size(), 1U);
  notification = observer_notifications_[0];
  ASSERT_EQ(notification.type, kDelete);
  EXPECT_EQ(notification.bucket_locator, bucket.ToBucketLocator());

  QuotaDatabase::SetClockForTesting(nullptr);
}
}  // namespace storage

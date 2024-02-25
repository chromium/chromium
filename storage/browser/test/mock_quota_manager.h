// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_
#define STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

namespace storage {

// Mocks the pieces of QuotaManager's interface.
//
// For usage/quota tracking test:
// Usage and quota information can be updated by following private helper
// methods: SetQuota() and UpdateUsage().
//
// For time-based deletion test:
// Storage keys can be added to the mock by calling AddStorageKey, and that list
// of storage keys is then searched through in GetStorageKeysModifiedBetween.
// Neither GetStorageKeysModifiedBetween nor DeleteStorageKeyData touches the
// actual storage key data stored in the profile.
class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(bool is_incognito,
                   const base::FilePath& profile_path,
                   scoped_refptr<base::SingleThreadTaskRunner> io_thread,
                   scoped_refptr<SpecialStoragePolicy> special_storage_policy);

  MockQuotaManager(const MockQuotaManager&) = delete;
  MockQuotaManager& operator=(const MockQuotaManager&) = delete;

  // Overrides QuotaManager's implementation that maintains an internal
  // container of created buckets and avoids going to the DB.
  void UpdateOrCreateBucket(
      const BucketInitParams& bucket_params,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  // Synchronous wrapper around `UpdateOrCreateBucket`, which overrides
  // QuotaManager's implementation that maintains an internal container of
  // created buckets and avoids going to the DB.
  // NOTE: the asynchronous version of this method `UpdateOrCreateBucket` is
  // preferred; only use this synchronous version where asynchronous bucket
  // retrieval is not possible.
  QuotaErrorOr<BucketInfo> GetOrCreateBucketSync(
      const BucketInitParams& params);

  // Overrides QuotaManager's implementation that maintains an internal
  // container of created buckets and avoids going to the DB.
  void GetOrCreateBucketDeprecated(
      const BucketInitParams& bucket_params,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  // Overrides QuotaManager's implementation to fetch from an internal
  // container populated by calls to GetOrCreateBucket.
  void GetBucketById(
      const BucketId& bucket_id,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  // Overrides QuotaManager's implementation to fetch from an internal
  // container populated by calls to GetOrCreateBucket.
  void GetBucketByNameUnsafe(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)>) override;

  void GetBucketsForStorageKey(
      const blink::StorageKey& storage_key,
      blink::mojom::StorageType type,
      base::OnceCallback<void(QuotaErrorOr<std::set<BucketInfo>>)> callback,
      bool delete_expired = false) override;

  // Overrides QuotaManager's implementation. The internal usage data is
  // updated when `MockQuotaManagerProxy::NotifyBucketModified()` is called. The
  // internal quota value can be updated by calling a helper method
  // `MockQuotaManager::SetQuota()`.
  void GetUsageAndQuota(const blink::StorageKey& storage_key,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override;

  int64_t GetQuotaForStorageKey(const blink::StorageKey& storage_key,
                                blink::mojom::StorageType type,
                                const QuotaSettings& settings) const override;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the storage key database that should be queried.
  // This method will only search through the storage keys added explicitly via
  // AddStorageKey.
  void GetBucketsModifiedBetween(blink::mojom::StorageType type,
                                 base::Time begin,
                                 base::Time end,
                                 GetBucketsCallback callback) override;

  // Removes a bucket from the canned list of buckets, but doesn't touch
  // anything on disk. The caller must provide `quota_client_types` which
  // specifies the types of QuotaClients which should be removed from this
  // bucket. Setting the mask to AllQuotaClientTypes() will remove all
  // clients from the bucket, regardless of type.
  void DeleteBucketData(const BucketLocator& bucket,
                        QuotaClientTypes quota_client_types,
                        StatusCallback callback) override;

  // Finds and removes a bucket from the canned list of buckets, but doesn't
  // touch anything on disk. Will remove bucket data from all QuotaClientTypes.
  // Will return kOk if deletion is successful or there is no bucket to delete.
  void FindAndDeleteBucketData(const blink::StorageKey& storage_key,
                               const std::string& bucket_name,
                               StatusCallback callback) override;

  void UpdateBucketPersistence(
      BucketId bucket,
      bool persistent,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  // Overrides QuotaManager's implementation so that tests can observe
  // calls to this function.
  void OnClientWriteFailed(const blink::StorageKey& storage_key) override;

  void CreateBucketForTesting(
      const blink::StorageKey& storage_key,
      const std::string& bucket_name,
      blink::mojom::StorageType storage_type,
      base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) override;

  // Helper method for updating internal quota info.
  void SetQuota(const blink::StorageKey& storage_key,
                blink::mojom::StorageType type,
                int64_t quota);

  // Helper methods for timed-deletion testing:
  // Adds a bucket to the canned list that will be searched through via
  // GetBucketsModifiedBetween.
  // `quota_clients` specified the types of QuotaClients this canned bucket
  // contains.
  bool AddBucket(const BucketInfo& bucket,
                 QuotaClientTypes quota_client_types,
                 base::Time modified);

  // Creates a BucketInfo object with a generated BucketId. Makes sure newly
  // created buckets are created with a unique id and with the specified
  // attributes.
  BucketInfo CreateBucket(const BucketInitParams& params,
                          blink::mojom::StorageType type);

  // Helper methods for timed-deletion testing:
  // Checks a bucket against the buckets that have been added via AddBucket and
  // removed via DeleteBucketData. If the bucket exists in the canned list with
  // the proper client, returns true.
  bool BucketHasData(const BucketInfo& bucket,
                     QuotaClientType quota_client_type) const;

  // Returns the count for how many buckets still exist for the client to make
  // sure there are no buckets that aren't accounted for during testing.
  int BucketDataCount(QuotaClientType quota_client);

  std::map<const blink::StorageKey, int> write_error_tracker() const {
    return write_error_tracker_;
  }

  void SetDisableDatabase(bool disable) { db_disabled_ = disable; }

 protected:
  ~MockQuotaManager() override;

 private:
  friend class MockQuotaManagerProxy;
  FRIEND_TEST_ALL_PREFIXES(MockQuotaManagerTest, QuotaAndUsage);

  // Contains the essential bits of information about a bucket that the
  // MockQuotaManager needs to understand for time-based deletion:
  // the bucket itself, the StorageType, its modification time and its
  // QuotaClients.
  struct BucketData {
    BucketData(const BucketInfo& bucket,
               QuotaClientTypes quota_clients,
               base::Time modified);
    ~BucketData();

    BucketData(const BucketData&) = delete;
    BucketData& operator=(const BucketData&) = delete;

    BucketData(BucketData&&);
    BucketData& operator=(BucketData&&);

    BucketInfo bucket;
    QuotaClientTypes quota_client_types;
    base::Time modified;
  };

  // Structure to support tracking quota per storage key.
  struct StorageKeyQuota {
    int64_t quota = std::numeric_limits<int64_t>::max();
  };

  // Structure to support tracking usage per bucket.
  struct BucketUsage {
    int64_t usage = 0;
  };

  QuotaErrorOr<BucketInfo> FindBucketById(const BucketId& bucket_id);

  QuotaErrorOr<BucketInfo> FindBucket(const blink::StorageKey& storage_key,
                                      const std::string& bucket_name,
                                      blink::mojom::StorageType type);

  QuotaErrorOr<BucketInfo> FindBucket(const BucketLocator& locator);

  QuotaErrorOr<BucketInfo> FindAndUpdateBucket(
      const BucketInitParams& bucket_params,
      blink::mojom::StorageType type);

  // This must be called via MockQuotaManagerProxy.
  void UpdateUsage(const BucketLocator& bucket, std::optional<int64_t> delta);

  void DidGetBucket(base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
                    QuotaErrorOr<BucketInfo> result);
  void DidGetModifiedInTimeRange(
      GetBucketsCallback callback,
      std::unique_ptr<std::set<BucketLocator>> buckets);
  void DidDeleteBucketData(StatusCallback callback,
                           blink::mojom::QuotaStatusCode status);

  base::FilePath profile_path() { return profile_path_; }

  const base::FilePath profile_path_;

  BucketId::Generator bucket_id_generator_;

  // The list of stored buckets that have been added via AddBucket.
  std::vector<BucketData> buckets_;
  std::map<std::pair<blink::StorageKey, blink::mojom::StorageType>,
           StorageKeyQuota>
      quota_map_;
  std::map<BucketLocator, BucketUsage, CompareBucketLocators> usage_map_;

  // Tracks number of times NotifyFailedWrite has been called per storage key.
  std::map<const blink::StorageKey, int> write_error_tracker_;

  bool db_disabled_ = false;

  base::WeakPtrFactory<MockQuotaManager> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_

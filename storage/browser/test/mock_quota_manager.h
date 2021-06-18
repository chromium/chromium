// Copyright 2014 The Chromium Authors. All rights reserved.
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

#include "base/macros.h"
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

  // Overrides QuotaManager's implementation. The internal usage data is
  // updated when MockQuotaManagerProxy::NotifyStorageModified() is
  // called.  The internal quota value can be updated by calling
  // a helper method MockQuotaManagerProxy::SetQuota().
  void GetUsageAndQuota(const blink::StorageKey& storage_key,
                        blink::mojom::StorageType type,
                        UsageAndQuotaCallback callback) override;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the storage key database that should be queried.
  // This method will only search through the storage keys added explicitly via
  // AddStorageKey.
  void GetStorageKeysModifiedBetween(blink::mojom::StorageType type,
                                     base::Time begin,
                                     base::Time end,
                                     GetStorageKeysCallback callback) override;

  // Removes an storage key from the canned list of storage keys, but doesn't
  // touch anything on disk. The caller must provide `quota_client_types` which
  // specifies the types of QuotaClients which should be removed from this
  // storage key. Setting the mask to AllQuotaClientTypes() will remove all
  // clients from the storage key, regardless of type.
  void DeleteStorageKeyData(const blink::StorageKey& storage_key,
                            blink::mojom::StorageType type,
                            QuotaClientTypes quota_client_types,
                            StatusCallback callback) override;

  // Overrides QuotaManager's implementation so that tests can observe
  // calls to this function.
  void NotifyWriteFailed(const blink::StorageKey& storage_key) override;

  // Helper method for updating internal quota info.
  void SetQuota(const blink::StorageKey& storage_key,
                blink::mojom::StorageType type,
                int64_t quota);

  // Helper methods for timed-deletion testing:
  // Adds a storage key to the canned list that will be searched through via
  // GetStorageKeysModifiedBetween.
  // `quota_clients` specified the types of QuotaClients this canned storage key
  // contains.
  bool AddStorageKey(const blink::StorageKey& storage_key,
                     blink::mojom::StorageType type,
                     QuotaClientTypes quota_client_types,
                     base::Time modified);

  // Helper methods for timed-deletion testing:
  // Checks an storage key and type against the storage keys that have been
  // added via AddStorageKey and removed via DeleteStorageKeyData. If the
  // storage key exists in the canned list with the proper StorageType and
  // client, returns true.
  bool StorageKeyHasData(const blink::StorageKey& storage_key,
                         blink::mojom::StorageType type,
                         QuotaClientType quota_client_type) const;

  std::map<const blink::StorageKey, int> write_error_tracker() const {
    return write_error_tracker_;
  }

 protected:
  ~MockQuotaManager() override;

 private:
  friend class MockQuotaManagerProxy;

  // Contains the essential bits of information about an storage key that the
  // MockQuotaManager needs to understand for time-based deletion:
  // the storage key itself, the StorageType and its modification time.
  struct StorageKeyInfo {
    StorageKeyInfo(const blink::StorageKey& storage_key,
                   blink::mojom::StorageType type,
                   QuotaClientTypes quota_clients,
                   base::Time modified);
    ~StorageKeyInfo();

    StorageKeyInfo(const StorageKeyInfo&) = delete;
    StorageKeyInfo& operator=(const StorageKeyInfo&) = delete;

    StorageKeyInfo(StorageKeyInfo&&);
    StorageKeyInfo& operator=(StorageKeyInfo&&);

    blink::StorageKey storage_key;
    blink::mojom::StorageType type;
    QuotaClientTypes quota_client_types;
    base::Time modified;
  };

  // Contains the essential information for each storage key for usage/quota
  // testing. (Ideally this should probably merged into the above struct, but
  // for regular usage/quota testing we hardly need modified time but only want
  // to keep usage and quota information, so this struct exists.
  struct StorageInfo {
    StorageInfo();
    ~StorageInfo();
    int64_t usage;
    int64_t quota;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
  };

  // This must be called via MockQuotaManagerProxy.
  void UpdateUsage(const blink::StorageKey& storage_key,
                   blink::mojom::StorageType type,
                   int64_t delta);
  void DidGetModifiedInTimeRange(
      GetStorageKeysCallback callback,
      std::unique_ptr<std::set<blink::StorageKey>> storage_keys,
      blink::mojom::StorageType storage_type);
  void DidDeleteStorageKeyData(StatusCallback callback,
                               blink::mojom::QuotaStatusCode status);

  // The list of stored storage keys that have been added via AddStorageKey.
  std::vector<StorageKeyInfo> storage_keys_;
  std::map<std::pair<blink::StorageKey, blink::mojom::StorageType>, StorageInfo>
      usage_and_quota_map_;

  // Tracks number of times NotifyFailedWrite has been called per storage key.
  std::map<const blink::StorageKey, int> write_error_tracker_;

  base::WeakPtrFactory<MockQuotaManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManager);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_TEST_MOCK_QUOTA_MANAGER_H_

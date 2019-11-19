// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_H_
#define CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_task.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using storage::GetOriginsCallback;
using storage::QuotaClient;
using storage::QuotaManager;
using storage::SpecialStoragePolicy;
using storage::StatusCallback;

namespace content {

// Mocks the pieces of QuotaManager's interface.
//
// For usage/quota tracking test:
// Usage and quota information can be updated by following private helper
// methods: SetQuota() and UpdateUsage().
//
// For time-based deletion test:
// Origins can be added to the mock by calling AddOrigin, and that list of
// origins is then searched through in GetOriginsModifiedSince.
// Neither GetOriginsModifiedSince nor DeleteOriginData touches the actual
// origin data stored in the profile.
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
  void GetUsageAndQuota(const url::Origin& origin,
                        StorageType type,
                        UsageAndQuotaCallback callback) override;

  // Overrides QuotaManager's implementation with a canned implementation that
  // allows clients to set up the origin database that should be queried. This
  // method will only search through the origins added explicitly via AddOrigin.
  void GetOriginsModifiedSince(StorageType type,
                               base::Time modified_since,
                               GetOriginsCallback callback) override;

  // Removes an origin from the canned list of origins, but doesn't touch
  // anything on disk. The caller must provide |quota_client_mask| which
  // specifies the types of QuotaClients which should be removed from this
  // origin as a bitmask built from QuotaClient::IDs. Setting the mask to
  // QuotaClient::kAllClientsMask will remove all clients from the origin,
  // regardless of type.
  void DeleteOriginData(const url::Origin& origin,
                        StorageType type,
                        int quota_client_mask,
                        StatusCallback callback) override;

  // Helper method for updating internal quota info.
  void SetQuota(const url::Origin& origin, StorageType type, int64_t quota);

  // Helper methods for timed-deletion testing:
  // Adds an origin to the canned list that will be searched through via
  // GetOriginsModifiedSince. The caller must provide |quota_client_mask|
  // which specifies the types of QuotaClients this canned origin contains
  // as a bitmask built from QuotaClient::IDs.
  bool AddOrigin(const url::Origin& origin,
                 StorageType type,
                 int quota_client_mask,
                 base::Time modified);

  // Helper methods for timed-deletion testing:
  // Checks an origin and type against the origins that have been added via
  // AddOrigin and removed via DeleteOriginData. If the origin exists in the
  // canned list with the proper StorageType and client, returns true.
  bool OriginHasData(const url::Origin& origin,
                     StorageType type,
                     QuotaClient::ID quota_client) const;

 protected:
  ~MockQuotaManager() override;

 private:
  friend class MockQuotaManagerProxy;

  // Contains the essential bits of information about an origin that the
  // MockQuotaManager needs to understand for time-based deletion:
  // the origin itself, the StorageType and its modification time.
  struct OriginInfo {
    OriginInfo(const url::Origin& origin,
               StorageType type,
               int quota_client_mask,
               base::Time modified);
    ~OriginInfo();

    url::Origin origin;
    StorageType type;
    int quota_client_mask;
    base::Time modified;
  };

  // Contains the essential information for each origin for usage/quota testing.
  // (Ideally this should probably merged into the above struct, but for
  // regular usage/quota testing we hardly need modified time but only
  // want to keep usage and quota information, so this struct exists.
  struct StorageInfo {
    StorageInfo();
    ~StorageInfo();
    int64_t usage;
    int64_t quota;
    blink::mojom::UsageBreakdownPtr usage_breakdown;
  };

  // This must be called via MockQuotaManagerProxy.
  void UpdateUsage(const url::Origin& origin, StorageType type, int64_t delta);
  void DidGetModifiedSince(GetOriginsCallback callback,
                           std::unique_ptr<std::set<url::Origin>> origins,
                           StorageType storage_type);
  void DidDeleteOriginData(StatusCallback callback,
                           blink::mojom::QuotaStatusCode status);

  // The list of stored origins that have been added via AddOrigin.
  std::vector<OriginInfo> origins_;
  std::map<std::pair<url::Origin, StorageType>, StorageInfo>
      usage_and_quota_map_;
  base::WeakPtrFactory<MockQuotaManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockQuotaManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_MOCK_QUOTA_MANAGER_H_

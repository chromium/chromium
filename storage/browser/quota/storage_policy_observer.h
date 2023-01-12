// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_STORAGE_POLICY_OBSERVER_H_
#define STORAGE_BROWSER_QUOTA_STORAGE_POLICY_OBSERVER_H_

#include <map>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/storage_policy_update.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "url/gurl.h"

namespace url {
class Origin;
}

namespace storage {

class StoragePolicyObserverIOThread;

// A helper class that aggregates storage::mojom::StoragePolicyUpdate
// changes for tracked origins. `StartTrackingOrigin()` adds an origin
// to start tracking.  When these origins have policy updates,
// the provided `callback` will be called with the policy update deltas.
class COMPONENT_EXPORT(STORAGE_BROWSER) StoragePolicyObserver {
 public:
  using ApplyPolicyUpdatesCallback = base::RepeatingCallback<void(
      std::vector<storage::mojom::StoragePolicyUpdatePtr>)>;

  StoragePolicyObserver(
      ApplyPolicyUpdatesCallback callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      scoped_refptr<storage::SpecialStoragePolicy> storage_policy);
  ~StoragePolicyObserver();

  StoragePolicyObserver(const StoragePolicyObserver&) = delete;
  StoragePolicyObserver& operator=(const StoragePolicyObserver&) = delete;

  // These methods are idempotent.  Tracking an origin that is already
  // tracked and stopping tracking an origin that is not being tracked
  // are noops.
  void StartTrackingOrigin(const url::Origin& origin);
  void StartTrackingOrigins(const std::vector<url::Origin>& origins);
  void StopTrackingOrigin(const url::Origin& origin);

  // Called by StoragePolicyObserverIOThread.
  void OnPolicyChanged();

  bool ShouldPurgeOnShutdownForTesting(const url::Origin& origin);

 private:
  bool ShouldPurgeOnShutdown(const GURL& origin);

  SEQUENCE_CHECKER(sequence_checker_);

  struct OriginState {
    // Indicates that storage for this origin should be purged on shutdown.
    bool should_purge_on_shutdown = false;
    // Indicates the last value for `purge_on_shutdown` that was communicated.
    bool will_purge_on_shutdown = false;
  };

  void OnPolicyChangedForOrigins(
      const std::vector<std::pair<const GURL, OriginState>*>& updated_origins);
  void AddPolicyUpdate(
      std::pair<const GURL, OriginState>* entry,
      std::vector<storage::mojom::StoragePolicyUpdatePtr>* policy_updates);

  // NOTE: The GURL key is specifically an origin GURL.
  // Special storage policy uses GURLs and not Origins, so it's simpler
  // to store everything in GURL form.
  std::map<GURL, OriginState> origin_state_;

  const ApplyPolicyUpdatesCallback callback_;
  const scoped_refptr<storage::SpecialStoragePolicy> storage_policy_;

  base::SequenceBound<StoragePolicyObserverIOThread> storage_policy_observer_;

  base::WeakPtrFactory<StoragePolicyObserver> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_STORAGE_POLICY_OBSERVER_H_

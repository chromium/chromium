// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_PENDING_UNSAFE_RESOURCE_STORAGE_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_PENDING_UNSAFE_RESOURCE_STORAGE_H_

#include <optional>

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_util.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#include "components/security_interstitials/core/unsafe_resource.h"

// Storage object that holds a copy of an UnsafeResource while its allow list
// decision is pending.  Once the pending decision for a resource is committed
// or discarded, the UnsafeResource is reset.
class PendingUnsafeResourceStorage {
 public:
  // PendingUnsafeResourceStorage is copyable.
  PendingUnsafeResourceStorage();
  PendingUnsafeResourceStorage(const PendingUnsafeResourceStorage& other);
  PendingUnsafeResourceStorage& operator=(
      const PendingUnsafeResourceStorage& other);
  ~PendingUnsafeResourceStorage();

  // Constructs a storage holding `resource`.
  explicit PendingUnsafeResourceStorage(
      const security_interstitials::UnsafeResource& resource);

  // Returns the pending UnsafeResource, or null if the pending decision is
  // finished.
  const security_interstitials::UnsafeResource* resource() const {
    return base::OptionalToPtr(resource_);
  }

 private:
  // Observer that updates the storage when the pending decision for its
  // resource is completed.
  class ResourcePolicyObserver : public SafeBrowsingUrlAllowList::Observer {
   public:
    ResourcePolicyObserver(PendingUnsafeResourceStorage* storage);
    ResourcePolicyObserver(const ResourcePolicyObserver& other);
    ResourcePolicyObserver& operator=(const ResourcePolicyObserver& other);
    ~ResourcePolicyObserver() override;

   private:
    // SafeBrowsingUrlAllowList::Observer:
    void ThreatPolicyUpdated(SafeBrowsingUrlAllowList* allow_list,
                             const GURL& url,
                             safe_browsing::SBThreatType threat_type,
                             SafeBrowsingUrlAllowList::Policy policy) override;
    void ThreatPolicyBatchUpdated(
        SafeBrowsingUrlAllowList* allow_list,
        const GURL& url,
        const std::set<safe_browsing::SBThreatType>& threat_types,
        SafeBrowsingUrlAllowList::Policy policy) override;
    void SafeBrowsingAllowListDestroyed(
        SafeBrowsingUrlAllowList* allow_list) override;

    raw_ptr<PendingUnsafeResourceStorage> storage_ = nullptr;
    base::ScopedObservation<SafeBrowsingUrlAllowList,
                            SafeBrowsingUrlAllowList::Observer>
        scoped_observation_{this};
  };

  // Updates `policy_observer_` for the current value of `resource_`.
  void UpdatePolicyObserver();

  // Resets `resource_` and destroys `policy_observer_`.
  void ResetResource();

  // The resource being stored.  Contains no value after the pending decision
  // has been either allowed or disallowed.
  std::optional<security_interstitials::UnsafeResource> resource_;
  // The observer for `resource_`'s pending decision.
  std::optional<ResourcePolicyObserver> policy_observer_;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_PENDING_UNSAFE_RESOURCE_STORAGE_H_

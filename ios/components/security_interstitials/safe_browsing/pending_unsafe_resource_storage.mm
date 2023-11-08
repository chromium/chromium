// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/pending_unsafe_resource_storage.h"

#import "base/containers/contains.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/ptr_util.h"
#import "ios/components/security_interstitials/safe_browsing/unsafe_resource_util.h"

using safe_browsing::SBThreatType;
using security_interstitials::UnsafeResource;

namespace {
// Returns whether a pending decision exists for `resource`.
bool IsUnsafeResourcePending(const UnsafeResource& resource) {
  SafeBrowsingUrlAllowList* allow_list = GetAllowListForResource(resource);
  GURL decision_url = SafeBrowsingUrlAllowList::GetDecisionUrl(resource);
  std::set<SBThreatType> pending_threat_types;
  return allow_list &&
         allow_list->IsUnsafeNavigationDecisionPending(decision_url,
                                                       &pending_threat_types) &&
         base::Contains(pending_threat_types, resource.threat_type);
}
}  // namespace

#pragma mark - PendingUnsafeResourceStorage

PendingUnsafeResourceStorage::PendingUnsafeResourceStorage() = default;

PendingUnsafeResourceStorage::PendingUnsafeResourceStorage(
    const PendingUnsafeResourceStorage& other)
    : resource_(other.resource_) {
  UpdatePolicyObserver();
}

PendingUnsafeResourceStorage& PendingUnsafeResourceStorage::operator=(
    const PendingUnsafeResourceStorage& other) {
  resource_ = other.resource_;
  UpdatePolicyObserver();
  return *this;
}

PendingUnsafeResourceStorage::~PendingUnsafeResourceStorage() = default;

PendingUnsafeResourceStorage::PendingUnsafeResourceStorage(
    const security_interstitials::UnsafeResource& resource)
    : resource_(resource) {
  DCHECK(IsUnsafeResourcePending(resource));
  // Reset the resource's callback to prevent misuse.
  resource_.value().callback = base::DoNothing();
  // Create the policy observer for `resource`.
  UpdatePolicyObserver();
}

#pragma mark Private

void PendingUnsafeResourceStorage::UpdatePolicyObserver() {
  if (resource_) {
    policy_observer_ = ResourcePolicyObserver(this);
  } else {
    policy_observer_ = std::nullopt;
  }
}

void PendingUnsafeResourceStorage::ResetResource() {
  resource_ = std::nullopt;
  policy_observer_ = std::nullopt;
}

#pragma mark - PendingUnsafeResourceStorage::ResourcePolicyObserver

PendingUnsafeResourceStorage::ResourcePolicyObserver::ResourcePolicyObserver(
    PendingUnsafeResourceStorage* storage)
    : storage_(storage) {
  scoped_observation_.Observe(SafeBrowsingUrlAllowList::FromWebState(
      storage_->resource()->weak_web_state.get()));
}

PendingUnsafeResourceStorage::ResourcePolicyObserver::ResourcePolicyObserver(
    const ResourcePolicyObserver& other)
    : storage_(other.storage_) {
  scoped_observation_.Observe(SafeBrowsingUrlAllowList::FromWebState(
      storage_->resource()->weak_web_state.get()));
}

PendingUnsafeResourceStorage::ResourcePolicyObserver&
PendingUnsafeResourceStorage::ResourcePolicyObserver::operator=(
    const ResourcePolicyObserver& other) {
  storage_ = other.storage_;
  return *this;
}

PendingUnsafeResourceStorage::ResourcePolicyObserver::
    ~ResourcePolicyObserver() = default;

void PendingUnsafeResourceStorage::ResourcePolicyObserver::ThreatPolicyUpdated(
    SafeBrowsingUrlAllowList* allow_list,
    const GURL& url,
    safe_browsing::SBThreatType threat_type,
    SafeBrowsingUrlAllowList::Policy policy) {
  const UnsafeResource* resource = storage_->resource();
  if (policy == SafeBrowsingUrlAllowList::Policy::kPending ||
      url != resource->navigation_url || threat_type != resource->threat_type) {
    return;
  }

  storage_->ResetResource();
  // ResetResource() destroys `this`, so no additional code should be added.
}

void PendingUnsafeResourceStorage::ResourcePolicyObserver::
    ThreatPolicyBatchUpdated(
        SafeBrowsingUrlAllowList* allow_list,
        const GURL& url,
        const std::set<safe_browsing::SBThreatType>& threat_types,
        SafeBrowsingUrlAllowList::Policy policy) {
  const UnsafeResource* resource = storage_->resource();
  if (policy == SafeBrowsingUrlAllowList::Policy::kPending ||
      url != resource->navigation_url ||
      !base::Contains(threat_types, resource->threat_type)) {
    return;
  }

  storage_->ResetResource();
  // ResetResource() destroys `this`, so no additional code should be added.
}

void PendingUnsafeResourceStorage::ResourcePolicyObserver::
    SafeBrowsingAllowListDestroyed(SafeBrowsingUrlAllowList* allow_list) {
  storage_->ResetResource();
  // ResetResource() destroys `this`, so no additional code should be added.
}

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_state.h"

namespace features {
const base::Feature kNewBrowsingContextStateOnBrowsingContextGroupSwap{
    "NewBrowsingContextStateOnBrowsingContextGroupSwap",
    base::FEATURE_DISABLED_BY_DEFAULT};

BrowsingContextStateImplementationType GetBrowsingContextMode() {
  if (base::FeatureList::IsEnabled(
          kNewBrowsingContextStateOnBrowsingContextGroupSwap)) {
    return BrowsingContextStateImplementationType::
        kSwapForCrossBrowsingInstanceNavigations;
  }

  return BrowsingContextStateImplementationType::
      kLegacyOneToOneWithFrameTreeNode;
}
}  // namespace features

namespace content {

BrowsingContextState::BrowsingContextState(
    blink::mojom::FrameReplicationStatePtr replication_state)
    : replication_state_(std::move(replication_state)) {}

BrowsingContextState::~BrowsingContextState() = default;

void BrowsingContextState::UpdateFramePolicy(
    const blink::FramePolicy& new_frame_policy,
    bool did_change_flags,
    bool did_change_container_policy,
    bool did_change_required_document_policy) {
  if (did_change_flags) {
    replication_state_->frame_policy.sandbox_flags =
        new_frame_policy.sandbox_flags;
  }
  if (did_change_container_policy) {
    replication_state_->frame_policy.container_policy =
        new_frame_policy.container_policy;
  }
  if (did_change_required_document_policy) {
    replication_state_->frame_policy.required_document_policy =
        new_frame_policy.required_document_policy;
  }
}

RenderFrameProxyHost* BrowsingContextState::GetRenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group) const {
  auto it = proxy_hosts_.find(site_instance_group->GetId());
  if (it != proxy_hosts_.end())
    return it->second.get();
  return nullptr;
}

void BrowsingContextState::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (origin.IsSameOriginWith(replication_state_->origin) &&
      replication_state_->has_potentially_trustworthy_unique_origin ==
          is_potentially_trustworthy_unique_origin) {
    return;
  }

  for (const auto& pair : proxy_hosts_) {
    pair.second->GetAssociatedRemoteFrame()->SetReplicatedOrigin(
        origin, is_potentially_trustworthy_unique_origin);
  }

  replication_state_->origin = origin;
  replication_state_->has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void BrowsingContextState::SetInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  if (policy == replication_state_->insecure_request_policy)
    return;
  for (const auto& pair : proxy_hosts_) {
    pair.second->GetAssociatedRemoteFrame()->EnforceInsecureRequestPolicy(
        policy);
  }
  replication_state_->insecure_request_policy = policy;
}

void BrowsingContextState::SetInsecureNavigationsSet(
    const std::vector<uint32_t>& insecure_navigations_set) {
  DCHECK(std::is_sorted(insecure_navigations_set.begin(),
                        insecure_navigations_set.end()));
  if (insecure_navigations_set == replication_state_->insecure_navigations_set)
    return;
  for (const auto& pair : proxy_hosts_) {
    pair.second->GetAssociatedRemoteFrame()->EnforceInsecureNavigationsSet(
        insecure_navigations_set);
  }
  replication_state_->insecure_navigations_set = insecure_navigations_set;
}

void BrowsingContextState::OnSetHadStickyUserActivationBeforeNavigation(
    bool value) {
  for (const auto& pair : proxy_hosts_) {
    pair.second->GetAssociatedRemoteFrame()
        ->SetHadStickyUserActivationBeforeNavigation(value);
  }
  replication_state_->has_received_user_gesture_before_nav = value;
}

void BrowsingContextState::SetIsAdSubframe(bool is_ad_subframe) {
  if (is_ad_subframe == replication_state_->is_ad_subframe)
    return;

  replication_state_->is_ad_subframe = is_ad_subframe;
  for (const auto& pair : proxy_hosts_) {
    pair.second->GetAssociatedRemoteFrame()->SetReplicatedIsAdSubframe(
        is_ad_subframe);
  }
}

void BrowsingContextState::ActiveFrameCountIsZero(
    SiteInstanceImpl* site_instance) {
  // |site_instance| no longer contains any active RenderFrameHosts, so we don't
  // need to maintain a proxy there anymore.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance->group());
  CHECK(proxy);

  DeleteRenderFrameProxyHost(site_instance);
}

void BrowsingContextState::RenderProcessGone(
    SiteInstanceImpl* instance,
    const ChildProcessTerminationInfo& info) {
  GetRenderFrameProxyHost(instance->group())->SetRenderFrameProxyCreated(false);
}

void BrowsingContextState::DeleteRenderFrameProxyHost(
    SiteInstance* site_instance) {
  static_cast<SiteInstanceImpl*>(site_instance)->RemoveObserver(this);
  proxy_hosts_.erase(
      static_cast<SiteInstanceImpl*>(site_instance)->group()->GetId());
}

}  // namespace content
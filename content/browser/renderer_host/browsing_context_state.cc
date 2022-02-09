// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_state.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

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

RenderFrameProxyHost* BrowsingContextState::GetRenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group) const {
  auto it = proxy_hosts_.find(site_instance_group->GetId());
  if (it != proxy_hosts_.end())
    return it->second.get();
  return nullptr;
}

bool BrowsingContextState::UpdateFramePolicyHeaders(
    network::mojom::WebSandboxFlags sandbox_flags,
    const blink::ParsedPermissionsPolicy& parsed_header) {
  bool changed = false;
  if (replication_state_->permissions_policy_header != parsed_header) {
    replication_state_->permissions_policy_header = parsed_header;
    changed = true;
  }
  // TODO(iclelland): Kill the renderer if sandbox flags is not a subset of the
  // currently effective sandbox flags from the frame. https://crbug.com/740556
  network::mojom::WebSandboxFlags updated_flags =
      sandbox_flags | replication_state_->frame_policy.sandbox_flags;
  if (replication_state_->active_sandbox_flags != updated_flags) {
    replication_state_->active_sandbox_flags = updated_flags;
    changed = true;
  }
  // Notify any proxies if the policies have been changed.
  if (changed) {
    for (const auto& pair : proxy_hosts_) {
      pair.second->GetAssociatedRemoteFrame()->DidSetFramePolicyHeaders(
          replication_state_->active_sandbox_flags,
          replication_state_->permissions_policy_header);
    }
  }
  return changed;
}

bool BrowsingContextState::CommitFramePolicy(
    const blink::FramePolicy& new_frame_policy) {
  // Documents create iframes, iframes host new documents. Both are associated
  // with sandbox flags. They are required to be stricter or equal to their
  // owner when they change, as we go down.
  // TODO(https://crbug.com/1262061). Enforce the invariant mentioned above,
  // once the interactions with fenced frame has been tested and clarified.

  bool did_change_flags = new_frame_policy.sandbox_flags !=
                          replication_state_->frame_policy.sandbox_flags;
  bool did_change_container_policy =
      new_frame_policy.container_policy !=
      replication_state_->frame_policy.container_policy;
  bool did_change_required_document_policy =
      new_frame_policy.required_document_policy !=
      replication_state_->frame_policy.required_document_policy;
  DCHECK_EQ(new_frame_policy.is_fenced,
            replication_state_->frame_policy.is_fenced);

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

  UpdateFramePolicyHeaders(new_frame_policy.sandbox_flags,
                           replication_state_->permissions_policy_header);
  return did_change_flags || did_change_container_policy ||
         did_change_required_document_policy;
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
    SiteInstanceGroup* site_instance_group) {
  // |site_instance_group| no longer contains any active RenderFrameHosts, so we
  // don't need to maintain a proxy there anymore.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance_group);
  CHECK(proxy);

  DeleteRenderFrameProxyHost(site_instance_group);
}

void BrowsingContextState::RenderProcessGone(
    SiteInstanceGroup* site_instance_group,
    const ChildProcessTerminationInfo& info) {
  GetRenderFrameProxyHost(site_instance_group)
      ->SetRenderFrameProxyCreated(false);
}

void BrowsingContextState::DeleteRenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group) {
  site_instance_group->RemoveObserver(this);
  proxy_hosts_.erase(site_instance_group->GetId());
}

void BrowsingContextState::SendFramePolicyUpdatesToProxies(
    SiteInstance* parent_site_instance,
    const blink::FramePolicy& frame_policy) {
  // Notify all of the frame's proxies about updated policies, excluding
  // the parent process since it already knows the latest state.
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->GetSiteInstance() != parent_site_instance) {
      pair.second->GetAssociatedRemoteFrame()->DidUpdateFramePolicy(
          frame_policy);
    }
  }
}

}  // namespace content
// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_state.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

namespace features {
BASE_FEATURE(kNewBrowsingContextStateOnBrowsingContextGroupSwap,
             "NewBrowsingContextStateOnBrowsingContextGroupSwap",
             base::FEATURE_DISABLED_BY_DEFAULT);

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

using perfetto::protos::pbzero::ChromeTrackEvent;

BrowsingContextState::BrowsingContextState(
    blink::mojom::FrameReplicationStatePtr replication_state,
    raw_ptr<RenderFrameHostImpl> parent,
    absl::optional<BrowsingInstanceId> browsing_instance_id)
    : replication_state_(std::move(replication_state)),
      parent_(parent),
      browsing_instance_id_(browsing_instance_id) {
  TRACE_EVENT_BEGIN("navigation", "BrowsingContextState",
                    perfetto::Track::FromPointer(this),
                    "browsing_context_state_when_created", this);
}

BrowsingContextState::~BrowsingContextState() {
  TRACE_EVENT_END("navigation", perfetto::Track::FromPointer(this));
}

RenderFrameProxyHost* BrowsingContextState::GetRenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group,
    ProxyAccessMode proxy_access_mode) const {
  TRACE_EVENT_BEGIN("navigation",
                    "BrowsingContextState::GetRenderFrameProxyHost",
                    ChromeTrackEvent::kBrowsingContextState, this,
                    ChromeTrackEvent::kSiteInstanceGroup, site_instance_group);
  auto* proxy =
      GetRenderFrameProxyHostImpl(site_instance_group, proxy_access_mode);
  TRACE_EVENT_END("navigation", ChromeTrackEvent::kRenderFrameProxyHost, proxy);
  return proxy;
}

RenderFrameProxyHost* BrowsingContextState::GetRenderFrameProxyHostImpl(
    SiteInstanceGroup* site_instance_group,
    ProxyAccessMode proxy_access_mode) const {
  if (features::GetBrowsingContextMode() ==
          features::BrowsingContextStateImplementationType::
              kSwapForCrossBrowsingInstanceNavigations &&
      proxy_access_mode == ProxyAccessMode::kRegular) {
    // CHECK to verify that the proxy is being accessed from the correct
    // BrowsingContextState. As both BrowsingContextState (in non-legacy mode)
    // and RenderFrameProxyHost (via SiteInstance) are tied to a given
    // BrowsingInstance, the browsing instance id of the BrowsingContextState
    // (in the non-legacy mode) and of the SiteInstanceGroup should match.
    // If they do not, the code calling this method has likely chosen the
    // wrong BrowsingContextGroup (e.g. one from the current RenderFrameHost
    // rather than from speculative or vice versa) – as this can lead to
    // various unpredictable bugs in proxy management logic, we want to
    // crash the browser here when this condition fails.
    //
    // Note that the outer delegate and opener proxies are an exception and the
    // only cases of a proxy associated with a SiteInstanceGroup from another
    // BrowsingInstance. Meanwhile, for openers the opener and openee have to be
    // in the same BrowsingInstance as well.
    CHECK_EQ(browsing_instance_id_.value(),
             site_instance_group->browsing_instance_id());
  }
  auto it = proxy_hosts_.find(site_instance_group->GetId());
  if (it != proxy_hosts_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void BrowsingContextState::DeleteRenderFrameProxyHost(
    SiteInstanceGroup* site_instance_group,
    ProxyAccessMode proxy_access_mode) {
  if (features::GetBrowsingContextMode() ==
          features::BrowsingContextStateImplementationType::
              kSwapForCrossBrowsingInstanceNavigations &&
      proxy_access_mode == ProxyAccessMode::kRegular) {
    // See comments in GetRenderFrameProxyHost for why this check is needed.
    CHECK_EQ(browsing_instance_id_.value(),
             site_instance_group->browsing_instance_id());
  }
  TRACE_EVENT("navigation", "BrowsingContextState::DeleteRenderFrameProxyHost",
              ChromeTrackEvent::kBrowsingContextState, this,
              ChromeTrackEvent::kSiteInstanceGroup, site_instance_group);
  site_instance_group->RemoveObserver(this);
  proxy_hosts_.erase(site_instance_group->GetId());
}

RenderFrameProxyHost* BrowsingContextState::CreateRenderFrameProxyHost(
    SiteInstance* site_instance,
    const scoped_refptr<RenderViewHostImpl>& rvh,
    FrameTreeNode* frame_tree_node,
    ProxyAccessMode proxy_access_mode,
    const blink::RemoteFrameToken& frame_token) {
  TRACE_EVENT_BEGIN(
      "navigation", "BrowsingContextState::CreateRenderFrameProxyHost",
      ChromeTrackEvent::kBrowsingContextState, this,
      ChromeTrackEvent::kSiteInstanceGroup,
      static_cast<SiteInstanceImpl*>(site_instance)->group(),
      ChromeTrackEvent::kRenderViewHost, rvh ? rvh.get() : nullptr,
      ChromeTrackEvent::kFrameTreeNodeInfo, frame_tree_node);

  if (features::GetBrowsingContextMode() ==
      features::BrowsingContextStateImplementationType::
          kLegacyOneToOneWithFrameTreeNode) {
    DCHECK_EQ(this,
              frame_tree_node->current_frame_host()->browsing_context_state());
  }

  if (features::GetBrowsingContextMode() ==
          features::BrowsingContextStateImplementationType::
              kSwapForCrossBrowsingInstanceNavigations &&
      proxy_access_mode == ProxyAccessMode::kRegular) {
    // See comments in GetRenderFrameProxyHost for why this check is needed.
    CHECK_EQ(browsing_instance_id_.value(),
             site_instance->GetBrowsingInstanceId());
  }

  auto site_instance_group_id =
      static_cast<SiteInstanceImpl*>(site_instance)->group()->GetId();
  CHECK(proxy_hosts_.find(site_instance_group_id) == proxy_hosts_.end())
      << "A proxy already existed for this SiteInstanceGroup.";
  RenderFrameProxyHost* proxy_host = new RenderFrameProxyHost(
      site_instance, std::move(rvh), frame_tree_node, frame_token);
  proxy_hosts_[site_instance_group_id] = base::WrapUnique(proxy_host);
  static_cast<SiteInstanceImpl*>(site_instance)->group()->AddObserver(this);

  TRACE_EVENT_END("navigation", ChromeTrackEvent::kRenderFrameProxyHost,
                  proxy_host);
  return proxy_host;
}

RenderFrameProxyHost* BrowsingContextState::CreateOuterDelegateProxy(
    SiteInstance* outer_contents_site_instance,
    FrameTreeNode* frame_tree_node,
    const blink::RemoteFrameToken& frame_token) {
  // We only get here when Delegate for this manager is an inner delegate.
  return CreateRenderFrameProxyHost(outer_contents_site_instance,
                                    /*rvh=*/nullptr, frame_tree_node,
                                    ProxyAccessMode::kAllowOuterDelegate,
                                    frame_token);
}

void BrowsingContextState::DeleteOuterDelegateProxy(
    SiteInstanceGroup* outer_contents_site_instance_group) {
  DeleteRenderFrameProxyHost(
      outer_contents_site_instance_group,
      BrowsingContextState::ProxyAccessMode::kAllowOuterDelegate);
}

size_t BrowsingContextState::GetProxyCount() {
  return proxy_hosts_.size();
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
    ExecuteRemoteFramesBroadcastMethod(
        base::BindRepeating(
            [](blink::mojom::FrameReplicationStatePtr& replication_state,
               RenderFrameProxyHost* proxy) {
              proxy->GetAssociatedRemoteFrame()->DidSetFramePolicyHeaders(
                  replication_state->active_sandbox_flags,
                  replication_state->permissions_policy_header);
            },
            std::ref(replication_state_)),
        /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
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

void BrowsingContextState::SetFrameName(const std::string& name,
                                        const std::string& unique_name) {
  if (name == replication_state_->name) {
    // |unique_name| shouldn't change unless |name| changes.
    DCHECK_EQ(unique_name, replication_state_->unique_name);
    return;
  }

  if (parent_) {
    // Non-main frames should have a non-empty unique name.
    DCHECK(!unique_name.empty());
  } else {
    // Unique name of main frames should always stay empty.
    DCHECK(unique_name.empty());
  }

  // Note the unique name should only be able to change before the first real
  // load is committed, but that's not strongly enforced here.
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](const std::string& name, const std::string& unique_name,
             RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()->SetReplicatedName(name,
                                                                 unique_name);
          },
          std::ref(name), std::ref(unique_name)),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
  replication_state_->unique_name = unique_name;
  replication_state_->name = name;
}

void BrowsingContextState::SetCurrentOrigin(
    const url::Origin& origin,
    bool is_potentially_trustworthy_unique_origin) {
  if (origin.IsSameOriginWith(replication_state_->origin) &&
      replication_state_->has_potentially_trustworthy_unique_origin ==
          is_potentially_trustworthy_unique_origin) {
    return;
  }

  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](const url::Origin& origin,
             bool is_potentially_trustworthy_unique_origin,
             RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()->SetReplicatedOrigin(
                origin, is_potentially_trustworthy_unique_origin);
          },
          std::ref(origin), std::ref(is_potentially_trustworthy_unique_origin)),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);

  replication_state_->origin = origin;
  replication_state_->has_potentially_trustworthy_unique_origin =
      is_potentially_trustworthy_unique_origin;
}

void BrowsingContextState::SetInsecureRequestPolicy(
    blink::mojom::InsecureRequestPolicy policy) {
  if (policy == replication_state_->insecure_request_policy)
    return;
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](blink::mojom::InsecureRequestPolicy policy,
             RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()->EnforceInsecureRequestPolicy(
                policy);
          },
          policy),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
  replication_state_->insecure_request_policy = policy;
}

void BrowsingContextState::SetInsecureNavigationsSet(
    const std::vector<uint32_t>& insecure_navigations_set) {
  DCHECK(std::is_sorted(insecure_navigations_set.begin(),
                        insecure_navigations_set.end()));
  if (insecure_navigations_set == replication_state_->insecure_navigations_set)
    return;
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](const std::vector<uint32_t>& insecure_navigations_set,
             RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()->EnforceInsecureNavigationsSet(
                insecure_navigations_set);
          },
          std::ref(insecure_navigations_set)),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
  replication_state_->insecure_navigations_set = insecure_navigations_set;
}

void BrowsingContextState::OnSetHadStickyUserActivationBeforeNavigation(
    bool value) {
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](bool value, RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()
                ->SetHadStickyUserActivationBeforeNavigation(value);
          },
          value),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
  replication_state_->has_received_user_gesture_before_nav = value;
}

void BrowsingContextState::SetIsAdFrame(bool is_ad_frame) {
  if (is_ad_frame == replication_state_->is_ad_frame)
    return;

  replication_state_->is_ad_frame = is_ad_frame;
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](bool is_ad_frame, RenderFrameProxyHost* proxy) {
            proxy->GetAssociatedRemoteFrame()->SetReplicatedIsAdFrame(
                is_ad_frame);
          },
          is_ad_frame),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
}

void BrowsingContextState::ActiveFrameCountIsZero(
    SiteInstanceGroup* site_instance_group) {
  // |site_instance_group| no longer contains any active RenderFrameHosts, so we
  // don't need to maintain a proxy there anymore.
  RenderFrameProxyHost* proxy = GetRenderFrameProxyHost(site_instance_group);
  CHECK(proxy);

  TRACE_EVENT_INSTANT("navigation",
                      "BrowsingContextState::ActiveFrameCountIsZero",
                      ChromeTrackEvent::kBrowsingContextState, this,
                      ChromeTrackEvent::kRenderFrameProxyHost, proxy);

  DeleteRenderFrameProxyHost(site_instance_group);
}

void BrowsingContextState::RenderProcessGone(
    SiteInstanceGroup* site_instance_group,
    const ChildProcessTerminationInfo& info) {
  GetRenderFrameProxyHost(site_instance_group,
                          ProxyAccessMode::kAllowOuterDelegate)
      ->SetRenderFrameProxyCreated(false);
}

void BrowsingContextState::SendFramePolicyUpdatesToProxies(
    SiteInstanceGroup* parent_group,
    const blink::FramePolicy& frame_policy) {
  // Notify all of the frame's proxies about updated policies, excluding
  // the parent process since it already knows the latest state.
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](SiteInstanceGroup* parent_group,
             const blink::FramePolicy& frame_policy,
             RenderFrameProxyHost* proxy) {
            if (proxy->site_instance_group() == parent_group)
              return;
            proxy->GetAssociatedRemoteFrame()->DidUpdateFramePolicy(
                frame_policy);
          },
          base::Unretained(parent_group), std::ref(frame_policy)),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
}

void BrowsingContextState::OnDidStartLoading() {
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating([](RenderFrameProxyHost* proxy) {
        proxy->GetAssociatedRemoteFrame()->DidStartLoading();
      }),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
}

void BrowsingContextState::OnDidStopLoading() {
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating([](RenderFrameProxyHost* proxy) {
        proxy->GetAssociatedRemoteFrame()->DidStopLoading();
      }),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
}

void BrowsingContextState::ResetProxyHosts() {
  for (const auto& pair : proxy_hosts_) {
    pair.second->site_instance_group()->RemoveObserver(this);
  }
  proxy_hosts_.clear();
}

void BrowsingContextState::UpdateOpener(
    SiteInstanceGroup* source_site_instance_group) {
  for (const auto& pair : proxy_hosts_) {
    if (pair.second->site_instance_group() == source_site_instance_group)
      continue;
    pair.second->UpdateOpener();
  }
}

void BrowsingContextState::OnDidUpdateFrameOwnerProperties(
    const blink::mojom::FrameOwnerProperties& properties) {
  // Notify this frame's proxies if they live in a different process from its
  // parent.  This is only currently needed for the allowFullscreen property,
  // since that can be queried on RemoteFrame ancestors.
  //
  // TODO(alexmos): It would be sufficient to only send this update to proxies
  // in the current FrameTree.
  ExecuteRemoteFramesBroadcastMethod(
      base::BindRepeating(
          [](SiteInstanceGroup* parent_group,
             const blink::mojom::FrameOwnerProperties& properties,
             RenderFrameProxyHost* proxy) {
            if (proxy->site_instance_group() == parent_group)
              return;
            proxy->GetAssociatedRemoteFrame()->SetFrameOwnerProperties(
                properties.Clone());
          },
          base::Unretained(parent_->GetSiteInstance()->group()),
          std::ref(properties)),
      /*instance_to_skip=*/nullptr, /*outer_delegate_proxy=*/nullptr);
}

void BrowsingContextState::ExecuteRemoteFramesBroadcastMethod(
    base::RepeatingCallback<void(RenderFrameProxyHost*)> callback,
    SiteInstance* instance_to_skip,
    RenderFrameProxyHost* outer_delegate_proxy) {
  for (const auto& pair : proxy_hosts_) {
    if (outer_delegate_proxy == pair.second.get())
      continue;
    if (pair.second->GetSiteInstance() == instance_to_skip)
      continue;
    if (!pair.second->is_render_frame_proxy_live())
      continue;
    callback.Run(pair.second.get());
  }
}

void BrowsingContextState::WriteIntoTrace(
    perfetto::TracedProto<TraceProto> proto) const {
  if (browsing_instance_id_.has_value())
    proto->set_browsing_instance_id(browsing_instance_id_.value().value());

  perfetto::TracedDictionary dict = std::move(proto).AddDebugAnnotations();
  dict.Add("this", static_cast<const void*>(this));
}

base::SafeRef<BrowsingContextState> BrowsingContextState::GetSafeRef() {
  return weak_factory_.GetSafeRef();
}

}  // namespace content

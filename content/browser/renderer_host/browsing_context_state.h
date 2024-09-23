// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/memory/safe_ref.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/security/coop/coop_related_group.h"
#include "content/browser/site_instance_group.h"
#include "content/public/browser/browsing_instance_id.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace features {
// Currently there are two paths - legacy code, in which BrowsingContextState
// will be 1:1 with FrameTreeNode, allowing us to move proxy storage to it as a
// no-op, and a new path hidden behind a feature flag, which will create a new
// BrowsingContextState for cross-BrowsingInstance navigations.

CONTENT_EXPORT extern const base::Feature
    kNewBrowsingContextStateOnBrowsingContextGroupSwap;

enum class BrowsingContextStateImplementationType {
  kLegacyOneToOneWithFrameTreeNode,
  kSwapForCrossBrowsingInstanceNavigations,
};

CONTENT_EXPORT BrowsingContextStateImplementationType GetBrowsingContextMode();
}  // namespace features

namespace content {

class RenderFrameHostImpl;

// BrowsingContextState is intended to store all state associated with a given
// browsing context (BrowsingInstance in the code, as defined in the HTML spec
// (https://html.spec.whatwg.org/multipage/browsers.html#browsing-context),
// in particular RenderFrameProxyHosts and FrameReplicationState. Each
// RenderFrameHost will have an associated BrowsingContextState (which never
// changes), but each BrowsingContextState can be shared between multiple
// RenderFrameHosts for the same frame/FrameTreeNode.

// BrowsingContextState is responsible for proxy storage and
// RenderFrameHostManager is responsible for connecting different
// BrowsingContextStates and creating proxies for appropriate SiteInstances.

// A new BCS will be created when a new RenderFrameHost is created for a new
// frame or a speculative RFH is created for a cross-BrowsingInstance (browsing
// context group in the spec) navigation (speculative RFHs created in the same
// BrowsingInstance will use the same BrowsingContextState as the old
// RenderFrameHost). For pages stored in bfcache and used for prerendering
// activations, BrowsingContextState will travel automatically together with the
// RenderFrameHost.

// Note: "browsing context" is an HTML spec term (close to a "frame") and it's
// different from content::BrowserContext, which represents a "browser profile".

// TODO(crbug.com/40205442): Currently it's under implementation and there are
// two different modes, controlled by a flag: kLegacyOneToOneWithFrameTreeNode,
// where BrowsingContextState is 1:1 with FrameTreeNode and exists for the
// duration of the FrameTreeNode lifetime, and
// kSwapForCrossBrowsingInstanceNavigations intended state with the behaviour
// described above, tied to the lifetime of the RenderFrameHostImpl.
// kLegacyOneToOneWithFrameTreeNode is currently enabled and will be removed
// once the functionality gated behind kSwapForCrossBrowsingInstanceNavigations
// is implemented.
class CONTENT_EXPORT BrowsingContextState
    : public base::RefCounted<BrowsingContextState>,
      public SiteInstanceGroup::Observer {
 public:
  using RenderFrameProxyHostMap =
      std::unordered_map<SiteInstanceGroupId,
                         std::unique_ptr<RenderFrameProxyHost>,
                         SiteInstanceGroupId::Hasher>;

  // Currently `browsing_instance_id` and `coop_related_group_id` will be null
  // iff the legacy mode is enabled, as the legacy mode BrowsingContextState is
  // 1:1 with FrameTreeNode and therefore doesn't have a dedicated associated
  // BrowsingInstance or CoopRelatedGroup.
  // TODO(crbug.com/40205442): Make `browsing_instance_id` and
  // `coop_related_group_id` non-optional when the legacy path is removed.
  BrowsingContextState(
      blink::mojom::FrameReplicationStatePtr replication_state,
      RenderFrameHostImpl* parent,
      std::optional<BrowsingInstanceId> browsing_instance_id,
      std::optional<base::UnguessableToken> coop_related_group_token);

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstanceGroup IDs, the values are RenderFrameProxyHosts.
  const RenderFrameProxyHostMap& proxy_hosts() const { return proxy_hosts_; }

  RenderFrameProxyHostMap& proxy_hosts() { return proxy_hosts_; }

  // Returns true if this is a main BrowsingContextState. True if and only if
  // this BrowsingContextState doesn't have a parent.
  bool is_main_frame() const { return !parent_; }

  const blink::mojom::FrameReplicationState& current_replication_state() const {
    return *replication_state_;
  }

  const std::string& frame_name() const { return replication_state_->name; }

  // Returns the currently active frame policy for this frame, including the
  // sandbox flags which were present at the time the document was loaded, and
  // the permissions policy container policy, which is set by the iframe's
  // allowfullscreen, allowpaymentrequest, and allow attributes, along with the
  // origin of the iframe's src attribute (which may be different from the URL
  // of the document currently loaded into the frame). This does not include
  // policy changes that have been made by updating the containing iframe
  // element attributes since the frame was last navigated; use
  // pending_frame_policy() for those.
  const blink::FramePolicy& effective_frame_policy() const {
    return replication_state_->frame_policy;
  }

  // Returns the sandbox flags currently in effect for this frame. This includes
  // flags inherited from parent frames, the currently active flags from the
  // <iframe> element hosting this frame, as well as any flags set from a
  // Content-Security-Policy HTTP header. This does not include flags that have
  // have been updated in an <iframe> element but have not taken effect yet; use
  // pending_frame_policy() for those. To see the flags which will take effect
  // on navigation (which does not include the CSP-set flags), use
  // effective_frame_policy().
  network::mojom::WebSandboxFlags active_sandbox_flags() const {
    return replication_state_->active_sandbox_flags;
  }

  void set_frame_name(const std::string& unique_name, const std::string& name) {
    replication_state_->unique_name = unique_name;
    replication_state_->name = name;
  }

  void set_has_active_user_gesture(bool has_active_user_gesture) {
    replication_state_->has_active_user_gesture = has_active_user_gesture;
  }

  // All proxies except outer delegate proxies should belong to the same
  // CoopRelatedGroup as their BrowsingContextState.
  //
  // When kSwapForCrossBrowsingInstanceNavigations is enabled, we might change
  // BrowsingContextState during a navigation. To ensure that we haven't mixed
  // up things, we CHECK that proxies are in the same CoopRelatedGroup. This
  // includes proxies in the BrowsingInstance as well as proxies for COOP:
  // restrict-properties related contexts. We do this CHECK in all functions for
  // creating, deleting, and accessing proxies. See
  // BrowsingContextState::GetRenderFrameProxyHostImpl() for an example.
  //
  // When we expect to be in one the exception cases we specify it via the
  // ProxyAccessMode enum below, which will disable the CHECKs.
  enum class ProxyAccessMode {
    kRegular,
    kAllowOuterDelegate,
  };

  RenderFrameProxyHost* GetRenderFrameProxyHost(
      SiteInstanceGroup* site_instance_group,
      ProxyAccessMode proxy_access_mode = ProxyAccessMode::kRegular) const;

  // Returns the number of RenderFrameProxyHosts for this frame.
  size_t GetProxyCount();

  // Set the current name and notify proxies about the update.
  void SetFrameName(const std::string& name, const std::string& unique_name);

  // Set the current origin and notify proxies about the update.
  void SetCurrentOrigin(const url::Origin& origin,
                        bool is_potentially_trustworthy_unique_origin);

  // Sets the current insecure request policy, and notifies proxies about the
  // update.
  void SetInsecureRequestPolicy(blink::mojom::InsecureRequestPolicy policy);

  // Sets the current set of insecure urls to upgrade, and notifies proxies
  // about the update.
  void SetInsecureNavigationsSet(
      const std::vector<uint32_t>& insecure_navigations_set);

  // Sets the sticky user activation status and notifies proxies about the
  // update.
  void OnSetHadStickyUserActivationBeforeNavigation(bool value);

  // Sets whether this is an ad frame and notifies the proxies about the update.
  void SetIsAdFrame(bool is_ad_frame);

  // Delete a RenderFrameProxyHost owned by this object.
  void DeleteRenderFrameProxyHost(
      SiteInstanceGroup* site_instance_group,
      ProxyAccessMode proxy_access_mode = ProxyAccessMode::kRegular);

  // SiteInstanceGroup::Observer
  void ActiveFrameCountIsZero(SiteInstanceGroup* site_instance_group) override;
  void KeepAliveCountIsZero(SiteInstanceGroup* site_instance_group) override;
  void RenderProcessGone(SiteInstanceGroup* site_instance_group,
                         const ChildProcessTerminationInfo& info) override;

  // Set the frame_policy provided in function parameter as active frame policy,
  // while leaving the FrameTreeNode::pending_frame_policy_ untouched. This
  // functionality is used on FrameTreeNode initialization, where it is
  // associated with a RenderFrameHost. Returns a boolean indicating whether
  // there was an update to the FramePolicy.
  bool CommitFramePolicy(const blink::FramePolicy& frame_policy);

  // Updates the active sandbox flags in this frame, in response to a
  // Content-Security-Policy header adding additional flags, in addition to
  // those given to this frame by its parent, or in response to the
  // Permissions-Policy header being set. Usually this will be when we create
  // WebContents with an opener. Note that on navigation, these updates will be
  // cleared, and the flags in the pending frame policy will be applied to the
  // frame. The old document's frame policy should therefore not impact the new
  // document's frame policy.
  // Returns true iff this operation has changed state of either sandbox flags
  // or permissions policy.
  bool UpdateFramePolicyHeaders(
      network::mojom::WebSandboxFlags sandbox_flags,
      const blink::ParsedPermissionsPolicy& parsed_header);

  // Notify all of the proxies about the updated FramePolicy, excluding the
  // parent, as it will already know.
  void SendFramePolicyUpdatesToProxies(SiteInstanceGroup* parent_group,
                                       const blink::FramePolicy& frame_policy);

  // Create a RenderFrameProxyHost owned by this object. This
  // RenderFrameProxyHost represents the browsing context in this
  // SiteInstanceGroup.
  // TODO(crbug.com/40205442): Currently we pass a FrameTreeNode because it is
  // required for the constructor to RenderFrameProxyHost. However, the stored
  // reference to FrameTreeNode should be replaced by a BrowsingContextState
  // instead; FrameTreeNode will need to be removed from here as well.
  RenderFrameProxyHost* CreateRenderFrameProxyHost(
      SiteInstanceGroup* site_instance_group,
      const scoped_refptr<RenderViewHostImpl>& rvh,
      FrameTreeNode* frame_tree_node,
      ProxyAccessMode proxy_access_mode = ProxyAccessMode::kRegular,
      const blink::RemoteFrameToken& frame_token = blink::RemoteFrameToken());

  // Called on the RFHM of the inner WebContents to create a
  // RenderFrameProxyHost in its outer WebContents' SiteInstanceGroup,
  // |outer_contents_site_instance_group|.
  RenderFrameProxyHost* CreateOuterDelegateProxy(
      SiteInstanceGroup* outer_contents_site_instance_group,
      FrameTreeNode* frame_tree_node,
      const blink::RemoteFrameToken& frame_token);

  // Deletes any proxy hosts associated with this node. Used during destruction
  // of WebContentsImpl.
  void ResetProxyHosts();

  // Notification methods to tell this RenderFrameHostManager that the frame it
  // is responsible for has started or stopped loading a document.
  void OnDidStartLoading();
  void OnDidStopLoading();

  // Notify proxies that an opener has been updated.
  void UpdateOpener(SiteInstanceGroup* source_site_instance_group);

  void OnDidUpdateFrameOwnerProperties(
      const blink::mojom::FrameOwnerProperties& properties);

  void ExecuteRemoteFramesBroadcastMethod(
      base::RepeatingCallback<void(RenderFrameProxyHost*)> callback,
      SiteInstanceGroup* group_to_skip,
      RenderFrameProxyHost* outer_delegate_proxy);

  using TraceProto = perfetto::protos::pbzero::BrowsingContextState;
  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedProto<TraceProto> proto) const;

  base::SafeRef<BrowsingContextState> GetSafeRef();

 protected:
  friend class base::RefCounted<BrowsingContextState>;

  ~BrowsingContextState() override;

 private:
  RenderFrameProxyHost* GetRenderFrameProxyHostImpl(
      SiteInstanceGroup* site_instance_group,
      ProxyAccessMode proxy_access_mode) const;

  // Helper to check if all refcounts SiteInstanceGroup keeps track of are zero.
  // Deletes all corresponding proxies if so. RefCountType is for tracing.
  enum RefCountType {
    kActiveFrameCount = 0,
    kKeepAliveCount = 1,
  };
  void CheckIfSiteInstanceGroupIsUnused(SiteInstanceGroup* site_instance_group,
                                        RefCountType ref_count_type);

  // Proxy hosts for this browsing context in various renderer processes, keyed
  // by SiteInstanceGroup ID.
  RenderFrameProxyHostMap proxy_hosts_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  blink::mojom::FrameReplicationStatePtr replication_state_;

  // Parent document of this BrowsingContextState, might be null if this is a
  // main frame BrowsingContextState.
  const raw_ptr<RenderFrameHostImpl> parent_;

  // ID of the BrowsingInstance and token of the CoopRelatedGroup to which this
  // BrowsingContextState belongs. Currently `browsing_instance_id` and
  // `coop_related_group_token` will be null iff the legacy mode is enabled, as
  // the legacy mode BrowsingContextState is 1:1 with FrameTreeNode and
  // therefore doesn't have a dedicated associated BrowsingInstance or
  // CoopRelatedGroup. TODO(crbug.com/40205442): Make `browsing_instance_id` and
  // `coop_related_group_token` non-optional when the legacy path is removed.
  const std::optional<BrowsingInstanceId> browsing_instance_id_;
  const std::optional<base::UnguessableToken> coop_related_group_token_;

  base::WeakPtrFactory<BrowsingContextState> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/site_instance_group.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom-forward.h"

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

// BrowsingContextState is intended to store all state associated with a given
// browsing context (BrowsingInstance in the code, as defined in the HTML spec
// (https://html.spec.whatwg.org/multipage/browsers.html#browsing-context),
// in particular RenderFrameProxyHosts and FrameReplicationState. Each
// RenderFrameHost will have an associated BrowsingContextState (which never
// changes), but each BrowsingContextState can be shared between multiple
// RenderFrameHosts for the same frame/FrameTreeNode.

// A new BCS will be created when a new RenderFrameHost is created for a new
// frame or a speculative RFH is created for a cross-BrowsingInstance (browsing
// context group in the spec) navigation (speculative RFHs created in the same
// BrowsingInstance will use the same BrowsingContextState as the old
// RenderFrameHost). For pages stored in bfcache and used for prerendering
// activations, BrowsingContextState will travel automatically together with the
// RenderFrameHost.

// Note: "browsing context" is an HTML spec term (close to a "frame") and it's
// different from content::BrowserContext, which represents a "browser profile".

// TODO(crbug.com/1270671): Currently it's under implementation and there are
// two different modes, controlled by a flag: kLegacyOneToOneWithFrameTreeNode,
// where BrowsingContextState is 1:1 with FrameTreeNode and exists for the
// duration of the FrameTreeNode lifetime, and
// kSwapForCrossBrowsingInstanceNavigations intended state with the behaviour
// described above, tied to the lifetime of the RenderFrameHostImpl.
// kLegacyOneToOneWithFrameTreeNode is currently enabled and will be removed
// once the functionality gated behind kSwapForCrossBrowsingInstanceNavigations
// is implemented.
class BrowsingContextState : public base::RefCounted<BrowsingContextState>,
                             public SiteInstanceGroup::Observer {
 public:
  using RenderFrameProxyHostMap =
      std::unordered_map<SiteInstanceGroupId,
                         std::unique_ptr<RenderFrameProxyHost>,
                         SiteInstanceGroupId::Hasher>;

  explicit BrowsingContextState(
      blink::mojom::FrameReplicationStatePtr replication_state);

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstanceGroup IDs, the values are RenderFrameProxyHosts.
  const RenderFrameProxyHostMap& proxy_hosts() const { return proxy_hosts_; }

  RenderFrameProxyHostMap& proxy_hosts() { return proxy_hosts_; }

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

  RenderFrameProxyHost* GetRenderFrameProxyHost(
      SiteInstanceGroup* site_instance_group) const;

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

  // Sets whether this is an ad subframe and notifies the proxies about the
  // update.
  void SetIsAdSubframe(bool is_ad_subframe);

  // Delete a RenderFrameProxyHost owned by this object.
  void DeleteRenderFrameProxyHost(SiteInstanceGroup* site_instance_group);

  // SiteInstanceGroup::Observer
  void ActiveFrameCountIsZero(SiteInstanceGroup* site_instance_group) override;
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
  void SendFramePolicyUpdatesToProxies(SiteInstance* parent_site_instance,
                                       const blink::FramePolicy& frame_policy);

 protected:
  friend class base::RefCounted<BrowsingContextState>;

  virtual ~BrowsingContextState();

 private:
  // Proxy hosts, indexed by SiteInstanceGroup ID.
  RenderFrameProxyHostMap proxy_hosts_;

  // Track information that needs to be replicated to processes that have
  // proxies for this frame.
  blink::mojom::FrameReplicationStatePtr replication_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

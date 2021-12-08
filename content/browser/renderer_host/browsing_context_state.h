// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

#include "base/memory/ref_counted.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
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
class BrowsingContextState : public base::RefCounted<BrowsingContextState> {
 public:
  using RenderFrameProxyHostMap =
      std::unordered_map<SiteInstanceGroupId,
                         std::unique_ptr<RenderFrameProxyHost>,
                         SiteInstanceGroupId::Hasher>;

  explicit BrowsingContextState();

  // Returns a const reference to the map of proxy hosts. The keys are
  // SiteInstanceGroup IDs, the values are RenderFrameProxyHosts.
  const RenderFrameProxyHostMap& proxy_hosts() const { return proxy_hosts_; }

  RenderFrameProxyHostMap& proxy_hosts() { return proxy_hosts_; }

 protected:
  friend class base::RefCounted<BrowsingContextState>;

  virtual ~BrowsingContextState();

 private:
  // Proxy hosts, indexed by SiteInstanceGroup ID.
  RenderFrameProxyHostMap proxy_hosts_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSING_CONTEXT_STATE_H_

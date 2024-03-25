// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_
#define CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_

#include <set>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "content/browser/site_instance_group.h"
#include "content/public/browser/site_instance.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {
class RenderFrameHostImpl;
class RenderFrameProxyHost;
class RenderViewHostImpl;

// StoredPage contains a page which is not tied to a FrameTree. It holds the
// main RenderFrameHost together with RenderViewHosts and main document's
// proxies. It's used for storing pages in back/forward cache or when preparing
// prerendered pages for activation. It is possible that this class
// is constructed with a `RenderViewHostImpl` that may disappear when a
// outstanding subframe navigation completes. To make sure the references
// of this class to `RenderViewHostImpl` are not stale this class implements
// `SiteInstanceGroup::Observer` to monitor their destruction.
class StoredPage : public SiteInstanceGroup::Observer {
 public:
  using RenderFrameProxyHostMap =
      std::unordered_map<SiteInstanceGroupId,
                         std::unique_ptr<RenderFrameProxyHost>,
                         SiteInstanceGroupId::Hasher>;

  using RenderViewHostImplSafeRefSet =
      std::set<base::SafeRef<RenderViewHostImpl>>;

  // A delegate class for various state change callbacks.
  class Delegate {
   public:
    // Callback to indicate when we've removed watching a RenderViewHostImpl.
    // This can happen when a RenderFrameProxyHost is no longer needed
    // and terminates.
    virtual void RenderViewHostNoLongerStored(RenderViewHostImpl* rvh) = 0;
  };

  StoredPage(std::unique_ptr<RenderFrameHostImpl> rfh,
             RenderFrameProxyHostMap proxy_hosts,
             RenderViewHostImplSafeRefSet render_view_hosts);
  ~StoredPage() override;

  void SetDelegate(Delegate* delegate);

  // SiteInstanceGroup::Observer overrides:
  void ActiveFrameCountIsZero(SiteInstanceGroup* site_instance_group) override;

  void SetPageRestoreParams(
      blink::mojom::PageRestoreParamsPtr page_restore_params) {
    page_restore_params_ = std::move(page_restore_params);
  }

  const blink::mojom::PageRestoreParamsPtr& page_restore_params() const {
    return page_restore_params_;
  }

  RenderFrameHostImpl* render_frame_host() { return render_frame_host_.get(); }

  const RenderViewHostImplSafeRefSet& render_view_hosts() const {
    return render_view_hosts_;
  }

  const StoredPage::RenderFrameProxyHostMap& proxy_hosts() const {
    return proxy_hosts_;
  }

  size_t proxy_hosts_size() { return proxy_hosts_.size(); }

  std::unique_ptr<RenderFrameHostImpl> TakeRenderFrameHost();

  // Must be called before `TakeProxyHosts()` or `TakeRenderViewHosts()`
  // is called.
  void PrepareToRestore();

  RenderFrameProxyHostMap TakeProxyHosts();
  RenderViewHostImplSafeRefSet TakeRenderViewHosts();

  void SetViewTransitionState(
      std::optional<blink::ViewTransitionState> view_transition_state);
  std::optional<blink::ViewTransitionState> TakeViewTransitionState();

 private:
  void ClearAllObservers();

  bool cleared_observers_ = false;

  // The main document being stored.
  std::unique_ptr<RenderFrameHostImpl> render_frame_host_;

  // Proxies of the main document as seen by other processes.
  // Currently, we only store proxies for SiteInstanceGroups of all subframes on
  // the page, because pages using window.open and nested WebContents are
  // not cached.
  RenderFrameProxyHostMap proxy_hosts_;

  // RenderViewHosts belonging to the main frames, and its proxies (if any).
  // Note this includes all `RenderViewHost`s from inner frame trees as well.
  //
  // While RenderViewHostImpl(s) are in the BackForwardCache, they aren't
  // reused for pages outside the cache. This prevents us from having two
  // main frames, (one in the cache, one live), associated with a single
  // RenderViewHost.
  //
  // Keeping these here also prevents RenderFrameHostManager code from
  // unwittingly iterating over RenderViewHostImpls that are in the cache.
  RenderViewHostImplSafeRefSet render_view_hosts_;

  // Additional parameters to send with SetPageLifecycleState calls when
  // we're restoring a page from the back-forward cache.
  blink::mojom::PageRestoreParamsPtr page_restore_params_;

  raw_ptr<Delegate> delegate_ = nullptr;

  // View transition state to use when the page is activated, either via BFCache
  // activation or prerender activation.
  std::optional<blink::ViewTransitionState> view_transition_state_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_STORED_PAGE_H_

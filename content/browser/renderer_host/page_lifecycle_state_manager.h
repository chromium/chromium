// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {

class RenderViewHostImpl;

// A class responsible for managing the main lifecycle state of the
// `blink::Page` and communicating in to the `blink::WebView`. 1:1 with
// `RenderViewHostImpl`.
class CONTENT_EXPORT PageLifecycleStateManager {
 public:
  class CONTENT_EXPORT TestDelegate {
   public:
    TestDelegate();
    virtual ~TestDelegate();
    virtual void OnLastAcknowledgedStateChanged(
        const blink::mojom::PageLifecycleState& old_state,
        const blink::mojom::PageLifecycleState& new_state);
    virtual void OnUpdateSentToRenderer(
        const blink::mojom::PageLifecycleState& new_state);
    virtual void OnDeleted();
  };

  explicit PageLifecycleStateManager(
      RenderViewHostImpl* render_view_host_impl,
      blink::mojom::PageVisibilityState frame_tree_visibility);
  ~PageLifecycleStateManager();

  void SetIsFrozen(bool frozen);
  void SetFrameTreeVisibility(
      blink::mojom::PageVisibilityState visibility_state);
  void SetIsInBackForwardCache(
      bool is_in_back_forward_cache,
      blink::mojom::PageRestoreParamsPtr page_restore_params);
  bool IsInBackForwardCache() const { return is_in_back_forward_cache_; }

  // Called when we're committing main-frame same-site navigations where we did
  // a proactive BrowsingInstance swap and we're reusing the old page's renderer
  // process, where we will run pagehide & visibilitychange handlers of the old
  // page from within the new page's commit call. Returns the newly updated
  // PageLifecycleState for this page (after we've set |pagehide_dispatch_| to
  // the appropriate value based on |persisted|).
  blink::mojom::PageLifecycleStatePtr SetPagehideDispatchDuringNewPageCommit(
      bool persisted);

  // See above, called when we've finished committing the new page (which means
  // we've finished running pagehide and visibilitychange handlers of the old
  // page) for certain cases.
  void DidSetPagehideDispatchDuringNewPageCommit(
      blink::mojom::PageLifecycleStatePtr acknowledged_state);

  // Calculates the per-page lifecycle state based on the per-tab / web contents
  // lifecycle state saved in this instance.
  blink::mojom::PageLifecycleStatePtr CalculatePageLifecycleState();

  const blink::mojom::PageLifecycleState& last_acknowledged_state() const {
    return *last_acknowledged_state_;
  }

  void SetIsLeavingBackForwardCache(base::OnceClosure done_cb);

  bool DidReceiveBackForwardCacheAck() const {
    return did_receive_back_forward_cache_ack_;
  }

  // Whether the renderer is expected to send channel associated IPCs related to
  // this page. E.g. while a page is in the back-forward cache the page should
  // be performing no work and thus not sending any IPCs.
  bool RendererExpectedToSendChannelAssociatedIpcs() const;

  void SetDelegateForTesting(TestDelegate* test_delegate_);

 private:
  // Send mojo message to renderer if the effective (page) lifecycle state has
  // changed.
  void SendUpdatesToRendererIfNeeded(
      blink::mojom::PageRestoreParamsPtr page_restore_params,
      base::OnceClosure done_cb);

  void OnPageLifecycleChangedAck(
      blink::mojom::PageLifecycleStatePtr acknowledged_state,
      base::OnceClosure done_cb);
  void OnBackForwardCacheTimeout();

  // This represents the frozen state set by |SetIsFrozen|, which corresponds to
  // WebContents::SetPageFrozen.  Effective frozen state, i.e. per-page frozen
  // state is computed based on |is_in_back_forward_cache_| and
  // |frozen_explicitly_|.
  bool frozen_explicitly_ = false;

  bool is_in_back_forward_cache_ = false;
  bool eviction_enabled_ = false;

  bool did_receive_back_forward_cache_ack_ = false;

  // This represents the frame tree visibility (same as web contents visibility
  // state for primary frame tree, hidden for prerendering frame tree) which is
  // set by |SetFrameTreeVisibility|. Effective visibility, i.e. per-page
  // visibility is computed based on |is_in_back_forward_cache_| and
  // |frame_tree_visibility_|.
  blink::mojom::PageVisibilityState frame_tree_visibility_;

  blink::mojom::PagehideDispatch pagehide_dispatch_ =
      blink::mojom::PagehideDispatch::kNotDispatched;

  const raw_ptr<RenderViewHostImpl> render_view_host_impl_;

  // This is the per-page state computed based on web contents / tab lifecycle
  // states, i.e. |frozen_explicitly_|, |is_in_back_forward_cache_| and
  // |frame_tree_visibility_|.
  blink::mojom::PageLifecycleStatePtr last_acknowledged_state_;

  // This is the per-page state that is sent to renderer most lately.
  blink::mojom::PageLifecycleStatePtr last_state_sent_to_renderer_;

  base::OneShotTimer back_forward_cache_timeout_monitor_;

  raw_ptr<TestDelegate> test_delegate_{nullptr};

  // NOTE: This must be the last member.
  base::WeakPtrFactory<PageLifecycleStateManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

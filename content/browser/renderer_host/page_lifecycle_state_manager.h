// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/browser/renderer_host/input/one_shot_timeout_monitor.h"
#include "content/common/content_export.h"
#include "content/public/common/page_visibility_state.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/page/page.mojom.h"

namespace content {

class RenderViewHostImpl;

// A class responsible for managing the main lifecycle state of the blink::Page
// and communicating in to the RenderView. 1:1 with RenderViewHostImpl.
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
  };

  explicit PageLifecycleStateManager(
      RenderViewHostImpl* render_view_host_impl,
      blink::mojom::PageVisibilityState web_contents_visibility_state);
  ~PageLifecycleStateManager();

  void SetIsFrozen(bool frozen);
  void SetWebContentsVisibility(
      blink::mojom::PageVisibilityState visibility_state);
  void SetIsInBackForwardCache(
      bool is_in_back_forward_cache,
      blink::mojom::PageRestoreParamsPtr page_restore_params);

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

  const blink::mojom::PageLifecycleState& last_state_sent_to_renderer() const {
    return *last_state_sent_to_renderer_;
  }

  void SetDelegateForTesting(TestDelegate* test_delegate_);

 private:
  // Send mojo message to renderer if the effective (page) lifecycle state has
  // changed.
  void SendUpdatesToRendererIfNeeded(
      blink::mojom::PageRestoreParamsPtr page_restore_params);

  void OnPageLifecycleChangedAck(
      blink::mojom::PageLifecycleStatePtr acknowledged_state);
  void OnBackForwardCacheTimeout();

  // This represents the frozen state set by |SetIsFrozen|, which corresponds to
  // WebContents::SetPageFrozen.  Effective frozen state, i.e. per-page frozen
  // state is computed based on |is_in_back_forward_cache_| and
  // |is_set_frozen_called_|.
  bool is_set_frozen_called_ = false;

  bool is_in_back_forward_cache_ = false;

  // This represents the visibility set by |SetVisibility|, which is web
  // contents visibility state. Effective visibility, i.e. per-page visibility
  // is computed based on |is_in_back_forward_cache_| and
  // |web_contents_visibility_|.
  blink::mojom::PageVisibilityState web_contents_visibility_;

  blink::mojom::PagehideDispatch pagehide_dispatch_ =
      blink::mojom::PagehideDispatch::kNotDispatched;

  RenderViewHostImpl* render_view_host_impl_;

  // This is the per-page state computed based on web contents / tab lifecycle
  // states, i.e. |is_set_frozen_called_|, |is_in_back_forward_cache_| and
  // |web_contents_visibility_|.
  blink::mojom::PageLifecycleStatePtr last_acknowledged_state_;

  // This is the per-page state that is sent to renderer most lately.
  blink::mojom::PageLifecycleStatePtr last_state_sent_to_renderer_;

  std::unique_ptr<OneShotTimeoutMonitor> back_forward_cache_timeout_monitor_;

  TestDelegate* test_delegate_{nullptr};
  // NOTE: This must be the last member.
  base::WeakPtrFactory<PageLifecycleStateManager> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PAGE_LIFECYCLE_STATE_MANAGER_H_

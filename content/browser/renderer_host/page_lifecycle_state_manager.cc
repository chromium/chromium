// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_lifecycle_state_manager.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {
// ASAN builds are slow and we see flakes caused by reaching this timeout. 6s
// was not enough to stop the flakes. Trying 12s just to ensure that there isn't
// something else going on that we don't understand.
// See https://crbug.com/1224355.
#if defined(ADDRESS_SANITIZER)
constexpr base::TimeDelta kBackForwardCacheTimeout = base::Seconds(12);
#else
constexpr base::TimeDelta kBackForwardCacheTimeout = base::Seconds(3);
#endif
base::TimeDelta GetBackForwardCacheEntryTimeout() {
  if (base::FeatureList::IsEnabled(features::kBackForwardCacheEntryTimeout)) {
    return kBackForwardCacheTimeout;
  } else {
    return base::TimeDelta::Max();
  }
}
}

namespace content {

PageLifecycleStateManager::TestDelegate::TestDelegate() = default;

PageLifecycleStateManager::TestDelegate::~TestDelegate() = default;

void PageLifecycleStateManager::TestDelegate::OnLastAcknowledgedStateChanged(
    const blink::mojom::PageLifecycleState& old_state,
    const blink::mojom::PageLifecycleState& new_state) {}

void PageLifecycleStateManager::TestDelegate::OnUpdateSentToRenderer(
    const blink::mojom::PageLifecycleState& new_state) {}

void PageLifecycleStateManager::TestDelegate::OnDeleted() {}

PageLifecycleStateManager::PageLifecycleStateManager(
    RenderViewHostImpl* render_view_host_impl,
    blink::mojom::PageVisibilityState frame_tree_visibility)
    : frame_tree_visibility_(frame_tree_visibility),
      render_view_host_impl_(render_view_host_impl) {
  last_acknowledged_state_ = CalculatePageLifecycleState();
  last_state_sent_to_renderer_ = last_acknowledged_state_.Clone();
}

PageLifecycleStateManager::~PageLifecycleStateManager() {
  if (test_delegate_)
    test_delegate_->OnDeleted();
}

void PageLifecycleStateManager::SetIsFrozen(bool frozen) {
  if (frozen_explicitly_ == frozen) {
    return;
  }
  frozen_explicitly_ = frozen;

  SendUpdatesToRendererIfNeeded(
      /*page_restore_params=*/nullptr, base::NullCallback());
}

void PageLifecycleStateManager::SetFrameTreeVisibility(
    blink::mojom::PageVisibilityState visibility) {
  if (frame_tree_visibility_ == visibility)
    return;

  frame_tree_visibility_ = visibility;

  if (visibility == blink::mojom::PageVisibilityState::kVisible) {
    // Unset `frozen_explicitly_` when the page is shown, to reflect that the
    // Blink page scheduler unfreezes the page in that situation. This ensures
    // that the page is frozen if SetIsFrozen(true) is called while the page is
    // hidden in the future (SetIsFrozen(true) no-ops if `frozen_explicitly_` is
    // true).
    frozen_explicitly_ = false;
  }

  SendUpdatesToRendererIfNeeded(
      /*page_restore_params=*/nullptr, base::NullCallback());
  // TODO(yuzus): When a page is frozen and made visible, the page should
  // automatically resume.
}

void PageLifecycleStateManager::SetIsInBackForwardCache(
    bool is_in_back_forward_cache,
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  if (is_in_back_forward_cache_ == is_in_back_forward_cache)
    return;
  // Prevent races by waiting for confirmation that the renderer will no longer
  // evict the page before allowing it to exit the back-forward cache
  DCHECK(is_in_back_forward_cache ||
         !last_acknowledged_state_->eviction_enabled);
  is_in_back_forward_cache_ = is_in_back_forward_cache;
  eviction_enabled_ = is_in_back_forward_cache;
  if (is_in_back_forward_cache) {
    // When a page is put into BackForwardCache, the page can run a busy loop.
    // Set a timeout monitor to check that the transition finishes within the
    // time limit.
    back_forward_cache_timeout_monitor_.Start(
        FROM_HERE, GetBackForwardCacheEntryTimeout(),
        base::BindOnce(&PageLifecycleStateManager::OnBackForwardCacheTimeout,
                       weak_ptr_factory_.GetWeakPtr()));
    pagehide_dispatch_ = blink::mojom::PagehideDispatch::kDispatchedPersisted;
  } else {
    DCHECK(page_restore_params);
    // When a page is restored from the back-forward cache, we should reset the
    // |pagehide_dispatch_| state so that we'd dispatch the
    // events again the next time we navigate away from the page.
    pagehide_dispatch_ = blink::mojom::PagehideDispatch::kNotDispatched;
  }

  SendUpdatesToRendererIfNeeded(std::move(page_restore_params),
                                base::NullCallback());
}

blink::mojom::PageLifecycleStatePtr
PageLifecycleStateManager::SetPagehideDispatchDuringNewPageCommit(
    bool persisted) {
  pagehide_dispatch_ =
      persisted ? blink::mojom::PagehideDispatch::kDispatchedPersisted
                : blink::mojom::PagehideDispatch::kDispatchedNotPersisted;
  // We've only modified |pagehide_dispatch_| here, but the "visibility"
  // property of |last_state_sent_to_renderer_| calculated from
  // CalculatePageLifecycleState() below will be set to kHidden because it
  // depends on the value of |pagehide_dispatch_|.
  last_state_sent_to_renderer_ = CalculatePageLifecycleState();
  DCHECK_EQ(last_state_sent_to_renderer_->visibility,
            blink::mojom::PageVisibilityState::kHidden);

  // We don't need to call SendUpdatesToRendererIfNeeded() because the update
  // will be sent through an OldPageInfo parameter in the CommitNavigation IPC.
  return last_state_sent_to_renderer_.Clone();
}

void PageLifecycleStateManager::DidSetPagehideDispatchDuringNewPageCommit(
    blink::mojom::PageLifecycleStatePtr acknowledged_state) {
  DCHECK_EQ(acknowledged_state->visibility,
            blink::mojom::PageVisibilityState::kHidden);
  DCHECK_NE(acknowledged_state->pagehide_dispatch,
            blink::mojom::PagehideDispatch::kNotDispatched);
  OnPageLifecycleChangedAck(std::move(acknowledged_state),
                            base::NullCallback());
}

void PageLifecycleStateManager::SetIsLeavingBackForwardCache(
    base::OnceClosure done_cb) {
  DCHECK(is_in_back_forward_cache_);
  eviction_enabled_ = false;
  SendUpdatesToRendererIfNeeded(nullptr, std::move(done_cb));
}

bool PageLifecycleStateManager::RendererExpectedToSendChannelAssociatedIpcs()
    const {
  // eviction_enabled_ => is_in_back_forward_cache_
  DCHECK(!eviction_enabled_ || is_in_back_forward_cache_);
  return !eviction_enabled_ || !last_acknowledged_state_->eviction_enabled;
}

void PageLifecycleStateManager::SendUpdatesToRendererIfNeeded(
    blink::mojom::PageRestoreParamsPtr page_restore_params,
    base::OnceClosure done_cb) {
  if (!render_view_host_impl_->GetAssociatedPageBroadcast()) {
    // TODO(crbug.com/40158974): For some tests, |render_view_host_impl_|
    // does not have the associated page.
    if (done_cb) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(done_cb));
    }
    return;
  }

  auto new_state = CalculatePageLifecycleState();
  if (last_state_sent_to_renderer_ &&
      last_state_sent_to_renderer_.Equals(new_state)) {
    // TODO(yuzus): Send updates to renderer only when the effective state (per
    // page lifecycle state) has changed since last sent to renderer. It is
    // possible that the web contents state has changed but the effective state
    // has not.
  }

  last_state_sent_to_renderer_ = new_state.Clone();
  auto state = new_state.Clone();

  if (test_delegate_)
    test_delegate_->OnUpdateSentToRenderer(*last_state_sent_to_renderer_);

  render_view_host_impl_->GetAssociatedPageBroadcast()->SetPageLifecycleState(
      std::move(state), std::move(page_restore_params),
      base::BindOnce(&PageLifecycleStateManager::OnPageLifecycleChangedAck,
                     weak_ptr_factory_.GetWeakPtr(), std::move(new_state),
                     std::move(done_cb)));
}

blink::mojom::PageLifecycleStatePtr
PageLifecycleStateManager::CalculatePageLifecycleState() {
  auto state = blink::mojom::PageLifecycleState::New();
  state->is_in_back_forward_cache = is_in_back_forward_cache_;
  state->is_frozen = is_in_back_forward_cache_ || frozen_explicitly_;
  state->pagehide_dispatch = pagehide_dispatch_;
  // If a page is stored in the back-forward cache, or we have already
  // dispatched/are dispatching pagehide for the page, it should be treated as
  // "hidden" regardless of what |frame_tree_visibility_| is set to.
  state->visibility =
      (is_in_back_forward_cache_ ||
       pagehide_dispatch_ != blink::mojom::PagehideDispatch::kNotDispatched)
          ? blink::mojom::PageVisibilityState::kHidden
          : frame_tree_visibility_;
  state->eviction_enabled = eviction_enabled_;
  return state;
}

void PageLifecycleStateManager::OnPageLifecycleChangedAck(
    blink::mojom::PageLifecycleStatePtr acknowledged_state,
    base::OnceClosure done_cb) {
  blink::mojom::PageLifecycleStatePtr old_state =
      std::move(last_acknowledged_state_);

  last_acknowledged_state_ = std::move(acknowledged_state);

  if (last_acknowledged_state_->is_in_back_forward_cache) {
    did_receive_back_forward_cache_ack_ = true;

    // TODO(crbug.com/41494183): currently after the navigation, the old
    // RenderViewHost is marked as inactive.
    // `RenderViewHostImpl::GetMainRenderFrameHost()` will return nullptr. This
    // prevents us from getting the RenderFrameHost even if the main frame of
    // this RenderViewHost is stored in BFCache. Now we are getting the
    // RenderFrameHost from the BackForwardCacheImpl as a workaround, but
    // eventually we might allow getting the RenderFrameHost from a
    // RenderViewHost that's in BFCache.
    for (auto* entry :
         render_view_host_impl_->frame_tree()
             ->controller()
             .GetBackForwardCache()
             .GetEntriesForRenderViewHostImpl(render_view_host_impl_)) {
      if (entry->render_frame_host()->LoadedWithCacheControlNoStoreHeader()) {
        // If the BFCached document was loaded with "Cache-control: no-store"
        // header, we clear the fallback surface and force the browser to embed
        // a completely new surface when this page is activated from BFCache.
        // This avoids displaying sensitive information between it's restored
        // and the `pageshow` handler completes.
        RenderWidgetHostViewBase* rwhv =
            render_view_host_impl_->GetWidget()->GetRenderWidgetHostViewBase();
        if (rwhv) {
          rwhv->InvalidateLocalSurfaceIdAndAllocationGroup();
          rwhv->ClearFallbackSurfaceForCommitPending();
        }
      }
    }
  }

  // Call |MaybeEvictFromBackForwardCache| after setting
  // |last_acknowledged_state_|.
  // Features which can be cleaned by the page are taken into account only
  // after the 'pagehide' handlers have run. As we might have just received
  // an acknowledgement from the renderer that these handlers have run, call
  // |MaybeEvictFromBackForwardCache| in case we need to start taking these
  // features into account.
  render_view_host_impl_->MaybeEvictFromBackForwardCache();

  // A page that has not yet received an acknowledgement from renderer is not
  // counted against the cache size limit because it might still be ineligible
  // for caching after the ack, i.e., after running handlers. After it receives
  // the ack and we call |MaybeEvictFromBackForwardCache()|, we know whether it
  // is eligible for caching and we should reconsider the cache size limits
  // again.
  render_view_host_impl_->EnforceBackForwardCacheSizeLimit();

  if (last_acknowledged_state_->is_in_back_forward_cache) {
    back_forward_cache_timeout_monitor_.Stop();
  }

  if (test_delegate_) {
    test_delegate_->OnLastAcknowledgedStateChanged(*old_state,
                                                   *last_acknowledged_state_);
  }
  if (done_cb)
    std::move(done_cb).Run();
}

void PageLifecycleStateManager::OnBackForwardCacheTimeout() {
  DCHECK(!last_acknowledged_state_->is_in_back_forward_cache);
  render_view_host_impl_->OnBackForwardCacheTimeout();
  back_forward_cache_timeout_monitor_.Stop();
}

void PageLifecycleStateManager::SetDelegateForTesting(
    PageLifecycleStateManager::TestDelegate* test_delegate) {
  DCHECK(!test_delegate_ || !test_delegate);
  test_delegate_ = test_delegate;
}

}  // namespace content

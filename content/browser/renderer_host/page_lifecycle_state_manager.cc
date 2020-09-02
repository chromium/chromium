// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/page_lifecycle_state_manager.h"

#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace {
constexpr base::TimeDelta kBackForwardCacheTimeoutInSeconds =
    base::TimeDelta::FromSeconds(3);
}

namespace content {

PageLifecycleStateManager::TestDelegate::TestDelegate() = default;

PageLifecycleStateManager::TestDelegate::~TestDelegate() = default;

void PageLifecycleStateManager::TestDelegate::OnLastAcknowledgedStateChanged(
    const blink::mojom::PageLifecycleState& old_state,
    const blink::mojom::PageLifecycleState& new_state) {}

void PageLifecycleStateManager::TestDelegate::OnUpdateSentToRenderer(
    const blink::mojom::PageLifecycleState& new_state) {}

PageLifecycleStateManager::PageLifecycleStateManager(
    RenderViewHostImpl* render_view_host_impl,
    blink::mojom::PageVisibilityState web_contents_visibility_state)
    : web_contents_visibility_(web_contents_visibility_state),
      render_view_host_impl_(render_view_host_impl) {
  last_acknowledged_state_ = CalculatePageLifecycleState();
  last_state_sent_to_renderer_ = last_acknowledged_state_.Clone();
}

PageLifecycleStateManager::~PageLifecycleStateManager() {
  DCHECK(!test_delegate_);
}

void PageLifecycleStateManager::SetIsFrozen(bool frozen) {
  if (is_set_frozen_called_ == frozen)
    return;
  is_set_frozen_called_ = frozen;

  SendUpdatesToRendererIfNeeded(/*page_restore_params=*/nullptr);
}

void PageLifecycleStateManager::SetWebContentsVisibility(
    blink::mojom::PageVisibilityState visibility) {
  if (web_contents_visibility_ == visibility)
    return;

  web_contents_visibility_ = visibility;
  SendUpdatesToRendererIfNeeded(/*page_restore_params=*/nullptr);
  // TODO(yuzus): When a page is frozen and made visible, the page should
  // automatically resume.
}

void PageLifecycleStateManager::SetIsInBackForwardCache(
    bool is_in_back_forward_cache,
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  if (is_in_back_forward_cache_ == is_in_back_forward_cache)
    return;
  is_in_back_forward_cache_ = is_in_back_forward_cache;
  if (is_in_back_forward_cache) {
    // When a page is put into BackForwardCache, the page can run a busy loop.
    // Set a timeout monitor to check that the transition finishes within the
    // time limit.
    back_forward_cache_timeout_monitor_ =
        std::make_unique<OneShotTimeoutMonitor>(
            base::BindOnce(
                &PageLifecycleStateManager::OnBackForwardCacheTimeout,
                weak_ptr_factory_.GetWeakPtr()),
            kBackForwardCacheTimeoutInSeconds);
    pagehide_dispatch_ = blink::mojom::PagehideDispatch::kDispatchedPersisted;
  } else {
    DCHECK(page_restore_params);
    // When a page is restored from the back-forward cache, we should reset the
    // |pagehide_dispatch_| state so that we'd dispatch the
    // events again the next time we navigate away from the page.
    pagehide_dispatch_ = blink::mojom::PagehideDispatch::kNotDispatched;
  }

  SendUpdatesToRendererIfNeeded(std::move(page_restore_params));
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
  OnPageLifecycleChangedAck(std::move(acknowledged_state));
}

void PageLifecycleStateManager::SendUpdatesToRendererIfNeeded(
    blink::mojom::PageRestoreParamsPtr page_restore_params) {
  if (!render_view_host_impl_->GetAssociatedPageBroadcast()) {
    // For some tests, |render_view_host_impl_| does not have the associated
    // page.
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
                     weak_ptr_factory_.GetWeakPtr(), std::move(new_state)));
}

blink::mojom::PageLifecycleStatePtr
PageLifecycleStateManager::CalculatePageLifecycleState() {
  auto state = blink::mojom::PageLifecycleState::New();
  state->is_in_back_forward_cache = is_in_back_forward_cache_;
  state->is_frozen = is_in_back_forward_cache_ ? true : is_set_frozen_called_;
  state->pagehide_dispatch = pagehide_dispatch_;
  // If a page is stored in the back-forward cache, or we have already
  // dispatched/are dispatching pagehide for the page, it should be treated as
  // "hidden" regardless of what |web_contents_visibility_| is set to.
  state->visibility =
      (is_in_back_forward_cache_ ||
       pagehide_dispatch_ != blink::mojom::PagehideDispatch::kNotDispatched)
          ? blink::mojom::PageVisibilityState::kHidden
          : web_contents_visibility_;
  return state;
}

void PageLifecycleStateManager::OnPageLifecycleChangedAck(
    blink::mojom::PageLifecycleStatePtr acknowledged_state) {
  blink::mojom::PageLifecycleStatePtr old_state =
      std::move(last_acknowledged_state_);
  last_acknowledged_state_ = std::move(acknowledged_state);

  if (last_acknowledged_state_->is_in_back_forward_cache) {
    back_forward_cache_timeout_monitor_.reset(nullptr);
  }

  if (test_delegate_) {
    test_delegate_->OnLastAcknowledgedStateChanged(*old_state,
                                                   *last_acknowledged_state_);
  }
}

void PageLifecycleStateManager::OnBackForwardCacheTimeout() {
  DCHECK(!last_acknowledged_state_->is_in_back_forward_cache);
  render_view_host_impl_->OnBackForwardCacheTimeout();
  back_forward_cache_timeout_monitor_.reset(nullptr);
}

void PageLifecycleStateManager::SetDelegateForTesting(
    PageLifecycleStateManager::TestDelegate* test_delegate) {
  DCHECK(!test_delegate_ || !test_delegate);
  test_delegate_ = test_delegate;
}

}  // namespace content

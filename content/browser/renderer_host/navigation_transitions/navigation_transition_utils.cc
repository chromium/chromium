// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"

#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {

namespace {

static gfx::Size g_output_size_for_test = gfx::Size();

static int g_num_copy_requests_issued_for_testing = 0;

void CacheScreenshotImpl(base::WeakPtr<NavigationControllerImpl> controller,
                         int navigation_entry_id,
                         const SkBitmap& bitmap) {
  if (!controller) {
    // The tab was destroyed by the time we receive the bitmap from the GPU.
    return;
  }

  NavigationEntryImpl* entry =
      controller->GetEntryWithUniqueID(navigation_entry_id);
  if (!entry) {
    // The entry was deleted by the time we received the bitmap from the GPU.
    // This can happen by clearing the session history, or when the
    // `NavigationEntry` was replaced or deleted, etc.
    return;
  }

  if (entry == controller->GetLastCommittedEntry()) {
    // TODO(https://crbug.com/1472395): We shouldn't cache the screenshot into
    // the navigation entry if the entry is re-navigated after we send out the
    // copy request. See the two cases below.
    //
    // Consider a fast swipe that triggers history navigation A->B->A, where the
    // second A commits before the GPU responds with the first screenshotting(A)
    // task. Currently `entry == controller->GetLastCommittedEntry()` guards
    // against this stale screenshot; however we should combine with the case
    // below and guard them together (see comments on the crbug).
    //
    // Consider a fast swipe that triggers history navigation A->B->A->B, where
    // the second B commits before the GPU responds with the first
    // screenshotting(A) task. We should discard A's screenshot because it is
    // stale. Currently the capture code does not handle this case. We need to
    // discard the stale screenshot.
    return;
  }

  if (bitmap.drawsNothing()) {
    // The GPU is not able to produce a valid bitmap. This is an error case.
    LOG(ERROR) << "Cannot generate a valid bitmap for entry "
               << entry->GetUniqueID() << " url " << entry->GetURL();
    return;
  }

  SkBitmap immutable_copy(bitmap);
  immutable_copy.setImmutable();

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      immutable_copy, entry->GetUniqueID());
  NavigationEntryScreenshotCache* cache =
      controller->GetNavigationEntryScreenshotCache();
  cache->SetScreenshot(entry, std::move(screenshot));
}

void CacheScreenshot(base::WeakPtr<NavigationControllerImpl> controller,
                     int navigation_entry_id,
                     const SkBitmap& bitmap) {
  // `CacheScreenshot`, as the callback for `CopyFromExactSurface`, is not
  // guaranteed to be executed on the same thread that it was submitted from
  // (browser's UI thread). Since `NavigationEntryScreenshotCache` can only be
  // accessed from the browser's UI thread, we explicitly post the caching task
  // onto the UI thread. See https://crbug.com/1217049 for more context.
  GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
      ->PostTask(FROM_HERE, base::BindOnce(&CacheScreenshotImpl, controller,
                                           navigation_entry_id, bitmap));
}

// We only want to capture screenshots for navigation entries reachable via
// session history navigations. Namely, we don't capture for navigations where
// the previous `NavigationEntry` will be either reloaded or replaced and
// deleted (e.g., `location.replace`, non-primary `FrameTree` navigations, etc).
bool CanTraverseToPreviousEntryAfterNavigation(
    const NavigationRequest& navigation_request) {
  if (navigation_request.GetReloadType() != ReloadType::NONE) {
    // We don't capture for reloads.
    return false;
  }

  if (navigation_request.common_params().should_replace_current_entry) {
    // If the `NavigationEntry` that's about to be committed will replace the
    // previous `NavigationEntry`, we can't traverse to the previous
    // `NavigationEntry` after that.
    // This excludes the first navigation of a tab that replaces the initial
    // `NavigationEntry`, since there is no page to go back to after the initial
    // navigation.
    return false;
  }

  // Navigations in the non-primary `FrameTree` will always replace/reload, as
  // they're guaranteed to only have a single entry for the session history.
  CHECK(navigation_request.frame_tree_node()->frame_tree().is_primary());

  return true;
}

// TODO(liuwilliam): remove it once all the TODOs are implemented.
bool ShouldCaptureForWorkInProgressConditions(
    const NavigationRequest& navigation_request) {
  // TODO(https://crbug.com/1420995): Support same-doc navigations. Make sure
  // to test the `history.pushState` and `history.replaceState` APIs.
  if (navigation_request.IsSameDocument()) {
    return false;
  }

  // TODO(https://crbug.com/1421377): Support subframe navigations.
  if (!navigation_request.IsInMainFrame()) {
    return false;
  }

  if (navigation_request.frame_tree_node()->frame_tree().IsPortal() ||
      navigation_request.frame_tree_node()
          ->GetParentOrOuterDocumentOrEmbedder()) {
    // No support for non-MPArch Portal and GuestView.
    //
    // TODO(https://crbug.com/1422733): We don't need to support capturing
    // Portals, but we should make sure the browser behaves correctly with
    // Portals enabled. When a portal activates, it takes over the session
    // histories from its predecessor. Currently the
    // `NavigationEntryScreenshotCache` and the manager does not support the
    // inheritable screenshots.
    return false;
  }

  // The capture API is currently called from `Navigator::DidNavigate`, which
  // causes early commit navigations to look like same-RFH navigations. These
  // early commit cases currently include navigations from crashed frames and
  // some initial navigations in tabs, neither of which need to have screenshots
  // captured.
  //
  // TODO(https://crbug.com/1473327): We will relocate the capture API callsite
  // into `RenderFrameHostManager::CommitPending`.
  bool is_same_rfh_or_early_commit = navigation_request.GetRenderFrameHost() ==
                                     navigation_request.frame_tree_node()
                                         ->render_manager()
                                         ->current_frame_host();
  if (is_same_rfh_or_early_commit) {
    // TODO(https://crbug.com/1445976): Screenshot capture for same-RFH
    // navigations can yield unexpected results because the
    // `viz::LocalSurfaceId` update is in a different IPC than navigation. We
    // will rely on RenderDocument to be enabled to all navigations.
    return false;
  }

  // TODO(https://crbug.com/1421007): Handle Android native view (e.g. NTP),
  // where the bitmap needs to be generated on the Android side. Move the
  // capture logic into `CaptureNavigationEntryScreenshot`.
  //
  // TODO(https://crbug.com/1474904): Test capturing for WebUI.

  return true;
}

// Purge any existing screenshots from the destination entry. Invalidate instead
// of overwriting here because the screenshot is stale and can't be used
// anymore in future navigations to this entry, as the document that's about to
// be loaded might have different contents than when the screenshot was taken in
// a previous load. A new screenshot should be taken when navigating away from
// this entry again.
void RemoveScreenshotFromDestination(
    const NavigationRequest& navigation_request) {
  NavigationEntry* destination_entry = navigation_request.GetNavigationEntry();

  if (!destination_entry) {
    // We don't always have a destination entry (e.g., a new (non-history)
    // subframe navigation). However if this is a session history navigation, we
    // most-likely have a destination entry to navigate toward, from which we
    // need to purge any existing screenshot.
    return;
  }

  NavigationControllerImpl& nav_controller =
      navigation_request.frame_tree_node()->navigator().controller();

  if (!nav_controller.frame_tree().is_primary()) {
    // Navigations in the non-primary FrameTree can still have a destination
    // entry (e.g., Prerender's initial document-fetch request will create a
    // pending entry), but they won't have a screenshot because the non-primary
    // FrameTree can't access the `NavigationEntryScreenshotCache`.
    CHECK_EQ(nav_controller.GetEntryCount(), 1);
    CHECK(!nav_controller.GetEntryAtIndex(0)->GetUserData(
        NavigationEntryScreenshot::kUserDataKey));
    return;
  }

  NavigationEntryScreenshotCache* cache =
      nav_controller.GetNavigationEntryScreenshotCache();
  if (destination_entry->GetUserData(NavigationEntryScreenshot::kUserDataKey)) {
    std::unique_ptr<NavigationEntryScreenshot> successfully_removed =
        cache->RemoveScreenshot(destination_entry);
    CHECK(successfully_removed);
  }
}
}  // namespace

void NavigationTransitionUtils::SetCapturedScreenshotSizeForTesting(
    const gfx::Size& size) {
  g_output_size_for_test = size;
}

int NavigationTransitionUtils::GetNumCopyOutputRequestIssuedForTesting() {
  return g_num_copy_requests_issued_for_testing;
}

void NavigationTransitionUtils::ResetNumCopyOutputRequestIssuedForTesting() {
  g_num_copy_requests_issued_for_testing = 0;
}

void NavigationTransitionUtils::CaptureNavigationEntryScreenshot(
    const NavigationRequest& navigation_request) {
  if (!AreBackForwardTransitionsEnabled()) {
    return;
  }

  // The current conditions for whether to capture a screenshot depend on
  // `NavigationRequest::GetRenderFrameHost()`, so for now we should only get
  // here after the `RenderFrameHost` has been selected for a successful
  // navigation.
  //
  // TODO(https://crbug.com/1473327): This CHECK won't hold for early-swap. For
  // early-swap, we don't have the network response when we swap the RFHs, thus
  // no RFH on the navigation request. See the comment above
  // `is_same_rfh_or_early_commit`.
  CHECK(navigation_request.HasRenderFrameHost());

  // Remove the screenshot from the destination before checking the conditions.
  // We might not capture for this navigation due to some conditions, but the
  // navigation still continues (to commit/finish), for which we need to remove
  // the screenshot from the destination entry.
  RemoveScreenshotFromDestination(navigation_request);

  if (!CanTraverseToPreviousEntryAfterNavigation(navigation_request)) {
    return;
  }

  // Temporarily check for cases that are not yet supported.
  if (!ShouldCaptureForWorkInProgressConditions(navigation_request)) {
    return;
  }

  //
  // The browser is guaranteed to issue the screenshot request beyond this.
  //

  // Without `SetOutputSizeForTest`, `g_output_size_for_test` is empty, meaning
  // we will capture at full-size, unless specified by tests.
  const gfx::Size output_size = g_output_size_for_test;

  RenderFrameHostImpl* current_rfh =
      navigation_request.frame_tree_node()->current_frame_host();
  RenderWidgetHostView* rwhv = current_rfh->GetView();
  CHECK(rwhv);
  // Make sure the browser is actively embedding a surface.
  CHECK(rwhv->IsSurfaceAvailableForCopy());
  NavigationControllerImpl& nav_controller =
      navigation_request.frame_tree_node()->navigator().controller();
  static_cast<RenderWidgetHostViewBase*>(rwhv)->CopyFromExactSurface(
      /*src_rect=*/gfx::Rect(), output_size,
      base::BindOnce(&CacheScreenshot, nav_controller.GetWeakPtr(),
                     nav_controller.GetLastCommittedEntry()->GetUniqueID()));

  ++g_num_copy_requests_issued_for_testing;
}

}  // namespace content

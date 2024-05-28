// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"

#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
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

// Construct a function-local variable instead of a standalone callback.
// Static local variables are first initialized when the function is first
// called (so we don't accidentally use this callback before it is even
// initialized); and base::NoDestructor makes sure the non-trivial destructor is
// not invoked.
NavigationTransitionUtils::ScreenshotCallback& GetTestScreenshotCallback() {
  static base::NoDestructor<NavigationTransitionUtils::ScreenshotCallback>
      instance;
  return *instance;
}

enum ShouldCapture {
  kNo,
  kOnlyAskEmbedder,
  kYes,
};

// Expect the following test methods to only be called if
// GetTestScreenshotCallback() is defined, and expect exactly one
// invocation for every call to CaptureNavigationEntryScreenshot.
// DO NOT invoke the test callback if the entry no longer exists.
void InvokeTestCallbackForNoScreenshot(
    const NavigationRequest& navigation_request) {
  SkBitmap override_unused;
  GetTestScreenshotCallback().Run(navigation_request.frame_tree_node()
                                      ->navigator()
                                      .controller()
                                      .GetLastCommittedEntryIndex(),
                                  {}, false, override_unused);
}

void InvokeTestCallback(int index,
                        const SkBitmap bitmap,
                        bool requested,
                        SkBitmap& override_bitmap) {
  SkBitmap test_copy(bitmap);
  test_copy.setImmutable();
  GetTestScreenshotCallback().Run(index, test_copy, requested, override_bitmap);
}

// Returns the first entry that matches `destination_token`. Returns null if no
// match is found.
NavigationEntryImpl* GetEntryForToken(
    NavigationControllerImpl* controller,
    const blink::SameDocNavigationScreenshotDestinationToken&
        destination_token) {
  for (int i = 0; i < controller->GetEntryCount(); ++i) {
    if (auto* entry = controller->GetEntryAtIndex(i);
        entry->same_document_navigation_entry_screenshot_token() ==
        destination_token) {
      return entry;
    }
  }
  return nullptr;
}

void CacheScreenshotImpl(NavigationControllerImpl& controller,
                         NavigationEntryImpl& entry,
                         bool is_copied_from_embedder,
                         const SkBitmap& bitmap) {
  auto navigation_entry_id = entry.GetUniqueID();

  if (&entry == controller.GetLastCommittedEntry()) {
    // TODO(crbug.com/40278616): We shouldn't cache the screenshot into
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

  SkBitmap bitmap_copy(bitmap);

  if (GetTestScreenshotCallback()) {
    SkBitmap override_bitmap;
    InvokeTestCallback(
        controller.GetEntryIndexWithUniqueID(navigation_entry_id), bitmap, true,
        override_bitmap);
    if (!override_bitmap.drawsNothing()) {
      bitmap_copy = override_bitmap;
    }
  }

  if (bitmap_copy.drawsNothing()) {
    // The GPU is not able to produce a valid bitmap. This is an error case.
    LOG(ERROR) << "Cannot generate a valid bitmap for entry "
               << entry.GetUniqueID() << " url " << entry.GetURL();
    return;
  }

  bitmap_copy.setImmutable();

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      bitmap_copy, entry.GetUniqueID(), is_copied_from_embedder);
  NavigationEntryScreenshotCache* cache =
      controller.GetNavigationEntryScreenshotCache();
  cache->SetScreenshot(&entry, std::move(screenshot));
}

void CacheScreenshotForCrossDocNavigations(
    base::WeakPtr<NavigationControllerImpl> controller,
    int navigation_entry_id,
    bool is_copied_from_embedder,
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
  CacheScreenshotImpl(*controller, *entry, is_copied_from_embedder, bitmap);
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
ShouldCapture ShouldCaptureForWorkInProgressConditions(
    const NavigationRequest& navigation_request) {
  // TODO(crbug.com/40259037): Support same-doc navigations. Make sure
  // to test the `history.pushState` and `history.replaceState` APIs.
  if (navigation_request.IsSameDocument()) {
    return ShouldCapture::kNo;
  }

  // TODO(crbug.com/40896219): Support subframe navigations.
  if (!navigation_request.IsInMainFrame()) {
    return ShouldCapture::kNo;
  }

  if (navigation_request.frame_tree_node()
          ->GetParentOrOuterDocumentOrEmbedder()) {
    // No support for embedded pages (including GuestView or fenced frames).
    return ShouldCapture::kNo;
  }

  // The capture API is currently called from `Navigator::DidNavigate`, which
  // causes early commit navigations to look like same-RFH navigations. These
  // early commit cases currently include navigations from crashed frames and
  // some initial navigations in tabs, neither of which need to have screenshots
  // captured.
  bool is_same_rfh_or_early_commit = navigation_request.GetRenderFrameHost() ==
                                     navigation_request.frame_tree_node()
                                         ->render_manager()
                                         ->current_frame_host();
  if (is_same_rfh_or_early_commit) {
    // TODO(crbug.com/40268383): Screenshot capture for same-RFH
    // navigations can yield unexpected results because the
    // `viz::LocalSurfaceId` update is in a different IPC than navigation. We
    // will rely on RenderDocument to be enabled to all navigations.
    return ShouldCapture::kOnlyAskEmbedder;
  }

  // TODO(crbug.com/40279439): Test capturing for WebUI.

  return ShouldCapture::kYes;
}

// Purge any existing screenshots from the destination entry. Invalidate instead
// of overwriting here because the screenshot is stale and can't be used
// anymore in future navigations to this entry, as the document that's about to
// be loaded might have different contents than when the screenshot was taken in
// a previous load. A new screenshot should be taken when navigating away from
// this entry again.
void RemoveScreenshotFromDestination(NavigationControllerImpl& nav_controller,
                                     NavigationEntry* destination_entry) {
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

void CacheScreenshotForSameDocNavigations(
    base::WeakPtr<NavigationControllerImpl> controller,
    int navigation_entry_id,
    const SkBitmap& bitmap) {
  CHECK(AreBackForwardTransitionsEnabled());

  if (!controller) {
    // The tab was destroyed by the time we receive the bitmap from the GPU.
    return;
  }

  auto* destination_entry =
      controller->GetEntryWithUniqueID(navigation_entry_id);

  if (!destination_entry) {
    // The entry was deleted by the time we received the bitmap from the GPU.
    // This can happen by clearing the session history, or when the
    // `NavigationEntry` was replaced or deleted, etc.
    return;
  }

  CacheScreenshotImpl(*controller, *destination_entry,
                      /*is_copied_from_embedder=*/false, bitmap);

  destination_entry->SetSameDocumentNavigationEntryScreenshotToken(
      std::nullopt);
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

void NavigationTransitionUtils::SetNavScreenshotCallbackForTesting(
    ScreenshotCallback screenshot_callback) {
  GetTestScreenshotCallback() = std::move(screenshot_callback);
}

void NavigationTransitionUtils::
    CaptureNavigationEntryScreenshotForCrossDocumentNavigations(
        const NavigationRequest& navigation_request) {
  if (!AreBackForwardTransitionsEnabled()) {
    return;
  }

  CHECK(!navigation_request.IsSameDocument());

  // The current conditions for whether to capture a screenshot depend on
  // `NavigationRequest::GetRenderFrameHost()`, so for now we should only get
  // here after the `RenderFrameHost` has been selected for a successful
  // navigation.
  //
  // TODO(crbug.com/40278956): This CHECK won't hold for early-swap. For
  // early-swap, we don't have the network response when we swap the RFHs, thus
  // no RFH on the navigation request. See the comment above
  // `is_same_rfh_or_early_commit`.
  CHECK(navigation_request.HasRenderFrameHost());

  auto* destination_entry = navigation_request.GetNavigationEntry();
  if (!destination_entry) {
    // We don't always have a destination entry (e.g., a new (non-history)
    // subframe navigation). However if this is a session history navigation, we
    // most-likely have a destination entry to navigate toward, from which we
    // need to purge any existing screenshot.
    return;
  }

  // Remove the screenshot from the destination before checking the conditions.
  // We might not capture for this navigation due to some conditions, but the
  // navigation still continues (to commit/finish), for which we need to remove
  // the screenshot from the destination entry.
  RemoveScreenshotFromDestination(
      navigation_request.frame_tree_node()->frame_tree().controller(),
      destination_entry);
  if (!CanTraverseToPreviousEntryAfterNavigation(navigation_request)) {
    if (GetTestScreenshotCallback()) {
      InvokeTestCallbackForNoScreenshot(navigation_request);
    }
    return;
  }

  // Temporarily check for cases that are not yet supported.
  // If we're navigating away from a crashed page, there's no web content to
  // capture. Only try to capture from the embedder.
  ShouldCapture should_capture =
      navigation_request.early_render_frame_host_swap_type() !=
              NavigationRequest::EarlyRenderFrameHostSwapType::kCrashedFrame
          ? ShouldCaptureForWorkInProgressConditions(navigation_request)
          : ShouldCapture::kOnlyAskEmbedder;
  if (should_capture == ShouldCapture::kNo) {
    if (GetTestScreenshotCallback()) {
      InvokeTestCallbackForNoScreenshot(navigation_request);
    }
    return;
  }

  NavigationControllerImpl& nav_controller =
      navigation_request.frame_tree_node()->navigator().controller();

  bool copied_via_delegate =
      navigation_request.GetDelegate()->MaybeCopyContentAreaAsBitmap(
          base::BindOnce(&CacheScreenshotForCrossDocNavigations,
                         nav_controller.GetWeakPtr(),
                         nav_controller.GetLastCommittedEntry()->GetUniqueID(),
                         /*is_copied_from_embedder=*/true));
  if (!copied_via_delegate &&
      should_capture == ShouldCapture::kOnlyAskEmbedder) {
    if (GetTestScreenshotCallback()) {
      InvokeTestCallbackForNoScreenshot(navigation_request);
    }
  }

  if (copied_via_delegate ||
      should_capture == ShouldCapture::kOnlyAskEmbedder) {
    return;
  }

  //
  // The browser is guaranteed to issue the screenshot request beyond this.
  //

  // Without `SetOutputSizeForTest`, `g_output_size_for_test` is empty,
  // meaning we will capture at full-size, unless specified by tests.
  const gfx::Size output_size = g_output_size_for_test;

  RenderFrameHostImpl* current_rfh =
      navigation_request.frame_tree_node()->current_frame_host();
  RenderWidgetHostView* rwhv = current_rfh->GetView();
  CHECK(rwhv);
  // Make sure the browser is actively embedding a surface.
  CHECK(rwhv->IsSurfaceAvailableForCopy());

  static_cast<RenderWidgetHostViewBase*>(rwhv)->CopyFromExactSurface(
      /*src_rect=*/gfx::Rect(), output_size,
      base::BindOnce(&CacheScreenshotForCrossDocNavigations,
                     nav_controller.GetWeakPtr(),
                     nav_controller.GetLastCommittedEntry()->GetUniqueID(),
                     /*is_copied_from_embedder=*/false));

  ++g_num_copy_requests_issued_for_testing;
}

void NavigationTransitionUtils::SetSameDocumentNavigationEntryScreenshotToken(
    const NavigationRequest& navigation_request,
    const blink::SameDocNavigationScreenshotDestinationToken&
        destination_token) {
  if (!AreBackForwardTransitionsEnabled()) {
    // The source of this call is from the renderer. We can't always trust the
    // renderer thus fail safely.
    return;
  }
  NavigationControllerImpl& nav_controller =
      navigation_request.frame_tree_node()->navigator().controller();
  if (GetEntryForToken(&nav_controller, destination_token)) {
    // Again, can't always trust the renderer to send a non-duplicated token.
    return;
  }

  CHECK(navigation_request.IsSameDocument());

  if (auto* destination_entry = navigation_request.GetNavigationEntry()) {
    RemoveScreenshotFromDestination(nav_controller, destination_entry);
  } else {
    // All renderer-initiated same-document navigations will not have a
    // destination entry (see
    // `NavigationRequest::CreateForSynchronousRendererCommit`).
  }

  if (!CanTraverseToPreviousEntryAfterNavigation(navigation_request)) {
    return;
  }

  // NOTE: `destination_token` is to set on the last committed entry (the
  // screenshot's destination), instead of the destination entry of this
  // `navigation_request` (`navigation_request.GetNavigationEntry()`).

  // We won't reach here if the renderer hasn't requested a CopyOutputRequest,
  // since the token in the DidCommitSameDocNavigation message will be nullopt.
  ++g_num_copy_requests_issued_for_testing;

  // `blink::SameDocNavigationScreenshotDestinationToken` is guaranteed
  // non-empty.
  nav_controller.GetLastCommittedEntry()
      ->SetSameDocumentNavigationEntryScreenshotToken(destination_token);

  CHECK(GetHostFrameSinkManager());

  GetHostFrameSinkManager()->SetOnCopyOutputReadyCallback(
      destination_token,
      base::BindOnce(&CacheScreenshotForSameDocNavigations,
                     nav_controller.GetWeakPtr(),
                     nav_controller.GetLastCommittedEntry()->GetUniqueID()));
}

}  // namespace content

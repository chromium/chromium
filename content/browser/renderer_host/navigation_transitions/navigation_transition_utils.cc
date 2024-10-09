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
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/gfx/animation/animation.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#endif

namespace content {

namespace {

using CacheHitOrMissReason = NavigationTransitionData::CacheHitOrMissReason;

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

// Expect the following test methods to only be called if
// GetTestScreenshotCallback() is defined, and expect exactly one
// invocation for every call to CaptureNavigationEntryScreenshot.
// DO NOT invoke the test callback if the entry no longer exists.
void InvokeTestCallbackForNoScreenshot(
    const NavigationRequest& navigation_request) {
  if (!GetTestScreenshotCallback()) {
    return;
  }

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

bool SupportsETC1NonPowerOfTwo(const NavigationRequest& navigation_request) {
#if BUILDFLAG(IS_ANDROID)
  auto* rfh = navigation_request.frame_tree_node()->current_frame_host();
  auto* rwhv = rfh->GetView();
  auto* window_android = rwhv->GetNativeView()->GetWindowAndroid();
  auto* compositor = window_android->GetCompositor();
  return static_cast<CompositorImpl*>(compositor)->SupportsETC1NonPowerOfTwo();
#else
  return false;
#endif
}

// Returns the first entry that matches `destination_token`. Returns null if no
// match is found.
NavigationEntryImpl* GetEntryForToken(
    NavigationControllerImpl* controller,
    const blink::SameDocNavigationScreenshotDestinationToken&
        destination_token) {
  for (int i = 0; i < controller->GetEntryCount(); ++i) {
    if (auto* entry = controller->GetEntryAtIndex(i);
        entry->navigation_transition_data()
            .same_document_navigation_entry_screenshot_token() ==
        destination_token) {
      return entry;
    }
  }
  return nullptr;
}

void CacheScreenshotImpl(base::WeakPtr<NavigationControllerImpl> controller,
                         base::WeakPtr<NavigationRequest> navigation_request,
                         int navigation_entry_id,
                         bool is_copied_from_embedder,
                         int copy_output_request_sequence,
                         bool supports_etc_non_power_of_two,
                         const SkBitmap& bitmap) {
  if (!controller) {
    // The tab was destroyed by the time we receive the bitmap from the GPU.
    return;
  }

  NavigationEntryImpl* entry =
      controller->GetEntryWithUniqueID(navigation_entry_id);
  if (!entry ||
      entry->navigation_transition_data().copy_output_request_sequence() !=
          copy_output_request_sequence) {
    // The entry has changed state since this request occurred so ignore it.
    return;
  }

  SkBitmap bitmap_copy(bitmap);

  if (GetTestScreenshotCallback()) {
    SkBitmap override_bitmap;
    InvokeTestCallback(
        controller->GetEntryIndexWithUniqueID(navigation_entry_id), bitmap,
        true, override_bitmap);
    if (!override_bitmap.drawsNothing()) {
      bitmap_copy = override_bitmap;
    }
  }

  if (bitmap_copy.drawsNothing()) {
    // The GPU is not able to produce a valid bitmap. This is an error case.
    LOG(ERROR) << "Cannot generate a valid bitmap for entry "
               << navigation_entry_id;
    if (entry) {
      entry->navigation_transition_data().set_cache_hit_or_miss_reason(
          CacheHitOrMissReason::kCapturedEmptyBitmap);
    }
    return;
  }

  bitmap_copy.setImmutable();

  auto screenshot = std::make_unique<NavigationEntryScreenshot>(
      bitmap_copy, navigation_entry_id, supports_etc_non_power_of_two);
  NavigationEntryScreenshotCache* cache =
      controller->GetNavigationEntryScreenshotCache();
  cache->SetScreenshot(std::move(navigation_request), std::move(screenshot),
                       is_copied_from_embedder);
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

bool CanInitiateCaptureForNavigationStage(
    const NavigationRequest& navigation_request,
    bool did_receive_commit_ack) {
  // We need to initiate the capture sooner for same-RFH navigations since
  // the RFH switches to rendering the new Document as soon as the navigation
  // commits in the renderer.
  // TODO(khushalsagar): This can be removed after RenderDocument.
  const bool is_same_render_frame_host =
      navigation_request.frame_tree_node()->current_frame_host() ==
      navigation_request.GetRenderFrameHost();

  if (is_same_render_frame_host) {
    return !did_receive_commit_ack;
  }

  return did_receive_commit_ack;
}

// Purge any existing screenshots from the destination entry. Invalidate instead
// of overwriting here because the screenshot is stale and can't be used
// anymore in future navigations to this entry, as the document that's about to
// be loaded might have different contents than when the screenshot was taken in
// a previous load. A new screenshot should be taken when navigating away from
// this entry again.
void RemoveScreenshotFromDestination(
    NavigationControllerImpl& navigation_controller,
    NavigationEntry* destination_entry) {
  if (!navigation_controller.frame_tree().is_primary()) {
    // Navigations in the non-primary FrameTree can still have a destination
    // entry (e.g., Prerender's initial document-fetch request will create a
    // pending entry), but they won't have a screenshot because the non-primary
    // FrameTree can't access the `NavigationEntryScreenshotCache`.
    CHECK_EQ(navigation_controller.GetEntryCount(), 1);
    CHECK(!navigation_controller.GetEntryAtIndex(0)->GetUserData(
        NavigationEntryScreenshot::kUserDataKey));
    return;
  }

  NavigationEntryScreenshotCache* cache =
      navigation_controller.GetNavigationEntryScreenshotCache();
  if (destination_entry->GetUserData(NavigationEntryScreenshot::kUserDataKey)) {
    std::unique_ptr<NavigationEntryScreenshot> successfully_removed =
        cache->RemoveScreenshot(destination_entry);
    CHECK(successfully_removed);
  }

  // Also ensure that any existing in-flight CopyOutputRequests will be
  // invalidated and their callbacks ignored. This ensures that new
  // CopyOutputRequests can be made without interference / double-caching.
  NavigationEntryImpl::FromNavigationEntry(destination_entry)
      ->navigation_transition_data()
      .increment_copy_output_request_sequence();
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

bool NavigationTransitionUtils::
    CaptureNavigationEntryScreenshotForCrossDocumentNavigations(
        NavigationRequest& navigation_request,
        bool did_receive_commit_ack) {
  if (!NavigationTransitionConfig::AreBackForwardTransitionsEnabled()) {
    return false;
  }

  CHECK(!navigation_request.IsSameDocument());

  if (!CanInitiateCaptureForNavigationStage(navigation_request,
                                            did_receive_commit_ack)) {
    return false;
  }

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
    return false;
  }

  NavigationControllerImpl& navigation_controller =
      navigation_request.frame_tree_node()->navigator().controller();
  auto* last_committed_entry = navigation_controller.GetLastCommittedEntry();

  // Remove the screenshot from the destination before checking the conditions.
  // We might not capture for this navigation due to some conditions, but the
  // navigation still continues (to commit/finish), for which we need to remove
  // the screenshot from the destination entry.
  RemoveScreenshotFromDestination(navigation_controller, destination_entry);

  if (gfx::Animation::PrefersReducedMotion()) {
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kCacheMissPrefersReducedMotion);
    InvokeTestCallbackForNoScreenshot(navigation_request);
    return false;
  }

  if (navigation_request.frame_tree_node()
          ->GetParentOrOuterDocumentOrEmbedder()) {
    // No support for embedded pages (including GuestView or fenced frames).
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kCacheMissEmbeddedPages);
    InvokeTestCallbackForNoScreenshot(navigation_request);
    return false;
  }

  if (!navigation_request.IsInPrimaryMainFrame()) {
    // See crbug.com/40896219: We will present the fallback UX for navigations
    // in the subframes.
    if (!last_committed_entry->navigation_transition_data()
             .cache_hit_or_miss_reason()
             .has_value()) {
      last_committed_entry->navigation_transition_data()
          .set_cache_hit_or_miss_reason(
              CacheHitOrMissReason::kCacheMissNonPrimaryMainFrame);
    }
    InvokeTestCallbackForNoScreenshot(navigation_request);
    return false;
  }

  if (navigation_request.frame_tree_node()
          ->current_frame_host()
          ->LoadedWithCacheControlNoStoreHeader()) {
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(CacheHitOrMissReason::kCacheMissCCNS);
    InvokeTestCallbackForNoScreenshot(navigation_request);
    return false;
  }

  if (!CanTraverseToPreviousEntryAfterNavigation(navigation_request)) {
    InvokeTestCallbackForNoScreenshot(navigation_request);
    return false;
  }

  bool only_use_embedder_screenshot = false;
  switch (navigation_request.early_render_frame_host_swap_type()) {
    case NavigationRequest::EarlyRenderFrameHostSwapType::kNone:
      break;
    case NavigationRequest::EarlyRenderFrameHostSwapType::kCrashedFrame:
      // If we're navigating away from a crashed frame, it's not possible to
      // get a screenshot and fallback UI should be used instead.
      InvokeTestCallbackForNoScreenshot(navigation_request);
      last_committed_entry->navigation_transition_data()
          .set_cache_hit_or_miss_reason(
              CacheHitOrMissReason::kNavigateAwayFromCrashedPage);
      return false;
    case NavigationRequest::EarlyRenderFrameHostSwapType::kInitialFrame:
      // TODO(khushalsagar): Confirm whether this is needed for Chrome's NTP
      // navigation.
      only_use_embedder_screenshot = true;
      break;
    case NavigationRequest::EarlyRenderFrameHostSwapType::kNavigationTransition:
      NOTREACHED();
  }

  RenderFrameHostImpl* current_rfh =
      navigation_request.frame_tree_node()->current_frame_host();
  RenderWidgetHostView* rwhv = current_rfh->GetView();
  if (!rwhv) {
    // The current frame is crashed but early swap didn't happen for this
    // navigation.
    CHECK(!current_rfh->IsRenderFrameLive());
    InvokeTestCallbackForNoScreenshot(navigation_request);
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kNavigateAwayFromCrashedPageNoEarlySwap);
    return false;
  }

#if BUILDFLAG(IS_ANDROID)
  if (auto* window_android = rwhv->GetNativeView()->GetWindowAndroid();
      !window_android || !window_android->GetCompositor()) {
    InvokeTestCallbackForNoScreenshot(navigation_request);
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kNoRootWindowOrCompositor);
    return false;
  }
#endif

  if (!rwhv->IsSurfaceAvailableForCopy()) {
    // See https://crbug.com/368289857: If we hide the WebContents after a
    // same-RFH navigation starts, we invalidate the `viz::LocalSurfaceID`
    // and the browser UI will not be embedding a new ID when the navigation
    // finishes (`WebContentsImpl::DidNavigateMainFramePreCommit()` and
    // `RenderWidgetHostViewAndroid::DidNavigate()`). We won't be able to
    // screenshot the page if we navigate the WebContents again before the UI
    // embeds
    InvokeTestCallbackForNoScreenshot(navigation_request);
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kBrowserNotEmbeddingValidSurfaceId);
    return false;
  }

  // https://crbug.com/369356401: It's possible to issue two CopyOutputRequests
  // against the last committed entry. Bump the `copy_output_request_sequence()`
  // to prevent double-caching the screenshot.
  last_committed_entry->navigation_transition_data()
      .increment_copy_output_request_sequence();
  int request_sequence = last_committed_entry->navigation_transition_data()
                             .copy_output_request_sequence();
  bool copied_via_delegate =
      navigation_request.GetDelegate()->MaybeCopyContentAreaAsBitmap(
          base::BindOnce(&CacheScreenshotImpl,
                         navigation_controller.GetWeakPtr(),
                         navigation_request.GetWeakPtr(),
                         last_committed_entry->GetUniqueID(),
                         /*is_copied_from_embedder=*/true, request_sequence,
                         SupportsETC1NonPowerOfTwo(navigation_request)));

  if (!copied_via_delegate && only_use_embedder_screenshot) {
    InvokeTestCallbackForNoScreenshot(navigation_request);
  }

  if (copied_via_delegate || only_use_embedder_screenshot) {
    return false;
  }

  //
  // The browser is guaranteed to issue the screenshot request beyond this.
  //

  // Without `SetOutputSizeForTest`, `g_output_size_for_test` is empty,
  // meaning we will capture at full-size, unless specified by tests.
  const gfx::Size output_size = g_output_size_for_test;

  static_cast<RenderWidgetHostViewBase*>(rwhv)->CopyFromExactSurface(
      /*src_rect=*/gfx::Rect(), output_size,
      base::BindOnce(&CacheScreenshotImpl, navigation_controller.GetWeakPtr(),
                     navigation_request.GetWeakPtr(),
                     last_committed_entry->GetUniqueID(),
                     /*is_copied_from_embedder=*/false, request_sequence,
                     SupportsETC1NonPowerOfTwo(navigation_request)));

  ++g_num_copy_requests_issued_for_testing;

  last_committed_entry->navigation_transition_data()
      .set_cache_hit_or_miss_reason(
          CacheHitOrMissReason::kSentScreenshotRequest);

  return true;
}

void NavigationTransitionUtils::SetSameDocumentNavigationEntryScreenshotToken(
    NavigationRequest& navigation_request,
    std::optional<blink::SameDocNavigationScreenshotDestinationToken>
        destination_token) {
  if (!NavigationTransitionConfig::AreBackForwardTransitionsEnabled()) {
    // The source of this call is from the renderer. We can't always trust the
    // renderer thus fail safely.
    return;
  }

  CHECK(navigation_request.IsSameDocument());

  NavigationControllerImpl& nav_controller =
      navigation_request.frame_tree_node()->navigator().controller();
  if (auto* destination_entry = navigation_request.GetNavigationEntry()) {
    RemoveScreenshotFromDestination(nav_controller, destination_entry);
  } else {
    // All renderer-initiated same-document navigations will not have a
    // destination entry (see
    // `NavigationRequest::CreateForSynchronousRendererCommit`).
  }

  // If the renderer sends a token, it implies it issued a copy request for the
  // pre-navigation state.
  if (destination_token) {
    ++g_num_copy_requests_issued_for_testing;
  }

  if (!CanTraverseToPreviousEntryAfterNavigation(navigation_request)) {
    return;
  }

  auto* last_committed_entry = nav_controller.GetLastCommittedEntry();
  if (gfx::Animation::PrefersReducedMotion()) {
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kCacheMissPrefersReducedMotion);
    return;
  }

  if (!destination_token) {
    return;
  }

  if (GetEntryForToken(&nav_controller, *destination_token)) {
    // Again, can't always trust the renderer to send a non-duplicated token.
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  RenderFrameHostImpl* current_rfh =
      navigation_request.frame_tree_node()->current_frame_host();
  RenderWidgetHostView* rwhv = current_rfh->GetView();
  if (auto* window_android = rwhv->GetNativeView()->GetWindowAndroid();
      !window_android || !window_android->GetCompositor()) {
    last_committed_entry->navigation_transition_data()
        .set_cache_hit_or_miss_reason(
            CacheHitOrMissReason::kNoRootWindowOrCompositor);
    return;
  }
#endif

  // NOTE: `destination_token` is to set on the last committed entry (the
  // screenshot's destination), instead of the destination entry of this
  // `navigation_request` (`navigation_request.GetNavigationEntry()`).

  // `blink::SameDocNavigationScreenshotDestinationToken` is guaranteed
  // non-empty.
  last_committed_entry->navigation_transition_data()
      .SetSameDocumentNavigationEntryScreenshotToken(*destination_token);

  CHECK(GetHostFrameSinkManager());

  // It is possible to issue two CopyOutputRequests against the last committed
  // entry. This happens when a same-RFH navigation commits in the browser at
  // the same time as a same-document navigation commits in the renderer. For
  // example,
  // 1. Browser has a navigation A->B. At ready to commit, browser sends a
  // screenshot request for A.
  // 2. Renderer commits a same-document navigation from A->A'. The renderer
  // issues a copy request for A at the same time as sending the commit message.
  // Bump the `copy_output_request_sequence()` to prevent double-caching the
  // screenshot for A.
  //
  // TODO(https://crbug.com/372301997): We will miss caching a screenshot for A'
  // in this case. Record that reason explicitly.
  last_committed_entry->navigation_transition_data()
      .increment_copy_output_request_sequence();
  int request_sequence = last_committed_entry->navigation_transition_data()
                             .copy_output_request_sequence();

  GetHostFrameSinkManager()->SetOnCopyOutputReadyCallback(
      *destination_token,
      base::BindOnce(&CacheScreenshotImpl, nav_controller.GetWeakPtr(),
                     navigation_request.GetWeakPtr(),
                     last_committed_entry->GetUniqueID(),
                     /*is_copied_from_embedder=*/false, request_sequence,
                     SupportsETC1NonPowerOfTwo(navigation_request)));
}

}  // namespace content

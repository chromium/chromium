// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/prerender_web_contents_delegate.h"

#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/preloading/prerender/prerender_host_registry.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {

void PrerenderWebContentsDelegate::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  // A prerendered page cannot open a new window.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::ActivateContents(WebContents* contents) {
  // WebContents should not be activated with this delegate.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::LoadingStateChanged(
    WebContents* source,
    bool should_show_loading_ui) {
  // Loading events should be deferred until prerender activation.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::CloseContents(WebContents* source) {
  // Cancelling prerendering should eventually destroy `this` and `source`.
  // However, this behavior is not implemented yet.
  // TODO(https://crbug.com/1499759): Implement this behavior.
  static_cast<WebContentsImpl*>(source)
      ->GetPrerenderHostRegistry()
      ->CancelAllHosts(PrerenderFinalStatus::kTabClosedWithoutUserGesture);
}

bool PrerenderWebContentsDelegate::ShouldSuppressDialogs(WebContents* source) {
  // Dialogs (JS dialogs and BeforeUnload confirm) should not be shown on a
  // prerendered page.
  NOTREACHED_NORETURN();
}

bool PrerenderWebContentsDelegate::ShouldFocusPageAfterCrash(
    WebContents* source) {
  // A prerendered page cannot be focused.
  return false;
}

bool PrerenderWebContentsDelegate::TakeFocus(WebContents* source,
                                             bool reverse) {
  // A prerendered page cannot be focused.
  return false;
}

void PrerenderWebContentsDelegate::WebContentsCreated(
    WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    WebContents* new_contents) {
  // A prerendered page should not create a new WebContents.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::PortalWebContentsCreated(
    WebContents* portal_web_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::WebContentsBecamePortal(
    WebContents* portal_web_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

bool PrerenderWebContentsDelegate::CanEnterFullscreenModeForTab(
    RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  // This should not be called for a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::EnterFullscreenModeForTab(
    RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  // This should not be called for a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::FullscreenStateChangedForTab(
    RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  // This should not be called for a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::ExitFullscreenModeForTab(WebContents*) {
  // This should not be called for a prerendered page.
  NOTREACHED_NORETURN();
}

bool PrerenderWebContentsDelegate::IsFullscreenForTabOrPending(
    const WebContents* web_contents) {
  return false;
}

void PrerenderWebContentsDelegate::OnDidBlockNavigation(
    WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  // DCHECK against LifecycleState in RenderFrameHostImpl::DidBlockNavigation()
  // ensures this is never called during prerendering.
  NOTREACHED_NORETURN();
}

PreloadingEligibility PrerenderWebContentsDelegate::IsPrerender2Supported(
    WebContents& web_contents) {
  // This should be checked in the initiator's WebContents.
  NOTREACHED_NORETURN();
}

std::unique_ptr<WebContents>
PrerenderWebContentsDelegate::ActivatePortalWebContents(
    WebContents* predecessor_contents,
    std::unique_ptr<WebContents> portal_contents) {
  // Portal is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

void PrerenderWebContentsDelegate::UpdateInspectedWebContentsIfNecessary(
    WebContents* old_contents,
    WebContents* new_contents,
    base::OnceCallback<void()> callback) {
  // This is called only for Portal that is not available on a prerendered page.
  NOTREACHED_NORETURN();
}

}  // namespace content

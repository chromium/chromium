// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents_based_canceller.h"

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "content/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

// static
std::unique_ptr<WebContentsBasedCanceller> WebContentsBasedCanceller::Create(
    RenderFrameHost* rfh,
    CancelCondition condition) {
  // `make_unique` would force the constructor to be public.
  auto canceller =
      base::WrapUnique(new WebContentsBasedCanceller(rfh, condition));
  if (canceller->CanShow()) {
    return canceller;
  }
  return nullptr;
}

WebContentsBasedCanceller::WebContentsBasedCanceller(
    RenderFrameHost* render_frame_host,
    CancelCondition condition)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      condition_(condition),
      document_(render_frame_host->GetWeakDocumentPtr()) {}

WebContentsBasedCanceller::~WebContentsBasedCanceller() = default;

bool WebContentsBasedCanceller::CanShow() {
  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  if (!render_frame_host) {
    return false;
  }
  return CanShowForVisibility(web_contents()->GetVisibility()) &&
         CanShowForRFHActiveState() && CanShowForTabState();
}

bool WebContentsBasedCanceller::CanShowForVisibility(Visibility visibility) {
  return condition_ != CancelCondition::kVisibility ||
         visibility != Visibility::HIDDEN;
}

bool WebContentsBasedCanceller::CanShowForRFHActiveState() {
  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  return render_frame_host && render_frame_host->IsActive();
}

bool WebContentsBasedCanceller::CanShowForTabState() {
  if (!base::FeatureList::IsEnabled(
          features::kSideBySideFilePickerCancelling)) {
    return true;
  }
  // Within Split View, it is possible for the tab containing a WebContents to
  // be visible but not active. This scenario is considered a cancel condition
  // for kVisibility rather than kActiveState because kActiveState is determined
  // by the RenderFrameHost state, while kVisibility is determined by the
  // WebContents state.
  WebContentsDelegate* web_contents_delegate = web_contents()->GetDelegate();
  return condition_ != CancelCondition::kVisibility || !web_contents_delegate ||
         web_contents_delegate->IsContentsActive(web_contents());
}

void WebContentsBasedCanceller::SetCancelCallback(
    CancelCallback cancel_callback) {
  CHECK(cancel_callback_.is_null());
  // Check all conditions immediately. This ensures that even if
  // SetCancelCallback is called later (in a different task), we are not leaving
  // a window for a race.
  if (!CanShow()) {
    std::move(cancel_callback).Run();
    return;
  }
  cancel_callback_ = std::move(cancel_callback);
}

void WebContentsBasedCanceller::OnVisibilityChanged(Visibility visibility) {
  // TODO(https://crbug.com/446032849): Remove this.
  VLOG(1) << "Visibility changed: " << static_cast<int>(visibility);
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/457495639): We need a different way to detect when a
  // WebContents is no longer displayed to the user for android since the
  // intent to select a file always causes a HIDDEN event as the whole app
  // receives onStop().
  return;
#else
  if (cancel_callback_.is_null()) {
    return;
  }
  if (!CanShowForVisibility(visibility)) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling";
    std::move(cancel_callback_).Run();
  }
#endif
}

void WebContentsBasedCanceller::RenderFrameHostStateChanged(
    RenderFrameHost* changed_render_frame_host,
    RenderFrameHost::LifecycleState old_state,
    RenderFrameHost::LifecycleState new_state) {
  // TODO(https://crbug.com/446032849): Remove this.
  VLOG(1) << "State changed: " << static_cast<int>(new_state);
  if (cancel_callback_.is_null()) {
    return;
  }

  if (!CanShowForRFHActiveState()) {
    VLOG(1) << "Cancelling.";
    std::move(cancel_callback_).Run();
    return;
  }
}

void WebContentsBasedCanceller::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  VLOG(1) << "Finished navigation";
  if (!document_.AsRenderFrameHostIfValid()) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling";
    std::move(cancel_callback_).Run();
  }
}

}  // namespace content

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents_based_canceller.h"

#include "base/memory/ptr_util.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
namespace content {

// static
std::unique_ptr<WebContentsBasedCanceller> WebContentsBasedCanceller::Create(
    RenderFrameHost* rfh,
    CancelCondition condition) {
  if (!rfh->IsActive()) {
    return nullptr;
  }
  WebContents* web_contents = WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    return nullptr;
  }
  if (condition == CancelCondition::kVisibility &&
      web_contents->GetVisibility() == Visibility::HIDDEN) {
    return nullptr;
  }
  // `make_unique` would force the constructor to be public.
  return base::WrapUnique(new WebContentsBasedCanceller(rfh, condition));
}

WebContentsBasedCanceller::WebContentsBasedCanceller(
    RenderFrameHost* render_frame_host,
    CancelCondition condition)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      condition_(condition),
      document_(render_frame_host->GetWeakDocumentPtr()) {}

WebContentsBasedCanceller::~WebContentsBasedCanceller() = default;

void WebContentsBasedCanceller::SetCancelCallback(
    CancelCallback cancel_callback) {
  CHECK(cancel_callback_.is_null());
  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  if (!render_frame_host) {
    // The document has been destroyed already.
    std::move(cancel_callback).Run();
    return;
  }
  cancel_callback_ = std::move(cancel_callback);
  // Trigger both handlers immediately. This ensures that even if
  // SetCancelCallback is called later (in a different task), we are not leaving
  // a window for a race.
  OnVisibilityChanged(web_contents()->GetVisibility());
  RenderFrameHostStateChanged(render_frame_host,
                              render_frame_host->GetLifecycleState(),
                              render_frame_host->GetLifecycleState());
}

void WebContentsBasedCanceller::OnVisibilityChanged(Visibility visibility) {
  // TODO(https://crbug.com/446032849): Remove this.
  VLOG(1) << "Visibility changed: " << static_cast<int>(visibility);
  if (cancel_callback_.is_null()) {
    return;
  }
  if (condition_ == CancelCondition::kVisibility &&
      visibility == Visibility::HIDDEN) {
#if BUILDFLAG(IS_ANDROID)
    // TODO(crbug.com/457495639): We need a different way to detect when a
    // WebContents is no longer displayed to the user for android since the
    // intent to select a file always causes a HIDDEN event as the whole app
    // receives onStop().
    VLOG(1) << "Ignoring for android";
#else
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling";
    std::move(cancel_callback_).Run();
#endif
  }
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

  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  if (!render_frame_host) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling.";
    std::move(cancel_callback_).Run();
    return;
  }
  if (changed_render_frame_host == render_frame_host &&
      !changed_render_frame_host->IsActive()) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling.";
    std::move(cancel_callback_).Run();
    return;
  }
}

void WebContentsBasedCanceller::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!document_.AsRenderFrameHostIfValid()) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling";
    std::move(cancel_callback_).Run();
  }
}

}  // namespace content

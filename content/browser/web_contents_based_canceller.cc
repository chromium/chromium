// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_based_canceller.h"

#include <optional>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace content {

#if !BUILDFLAG(IS_ANDROID)
namespace {

bool IsExemptFromSizeCheck(WebContents* outermost) {
  if (!outermost) {
    return true;
  }
  RenderFrameHost* rfh = outermost->GetPrimaryMainFrame();
  if (!rfh) {
    return true;
  }
  return GetContentClient()->browser()->IsExemptFromWindowSizeCheck(
      rfh->GetLastCommittedURL());
}

}  // namespace
#endif

// static
gfx::Size WebContentsBasedCanceller::GetTopLevelWindowSize(
    WebContents* web_contents) {
  if (!web_contents) {
    return gfx::Size();
  }
  WebContents* outermost = web_contents->GetOutermostWebContents();
  if (!outermost) {
    return gfx::Size();
  }
  if (WebContentsDelegate* delegate = outermost->GetDelegate()) {
    if (std::optional<gfx::Rect> bounds = delegate->GetWindowBoundsInScreen()) {
      return bounds->size();
    }
  }
  const gfx::Size contents_size = outermost->GetSize();
  if (!contents_size.IsEmpty()) {
    return contents_size;
  }
  if (RenderFrameHost* rfh = outermost->GetPrimaryMainFrame()) {
    if (RenderWidgetHostView* view = rfh->GetView()) {
      return view->GetViewBounds().size();
    }
  }
  return gfx::Size();
}

// static
bool WebContentsBasedCanceller::IsWindowTooSmall(
    WebContents* web_contents,
    const gfx::Size& min_window_size) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  if (!web_contents || min_window_size.IsEmpty()) {
    return false;
  }
  WebContents* outermost = web_contents->GetOutermostWebContents();
  if (IsExemptFromSizeCheck(outermost)) {
    return false;
  }
  const gfx::Size size = GetTopLevelWindowSize(outermost);
  if (size.IsEmpty()) {
    return true;
  }
  return size.width() < min_window_size.width() ||
         size.height() < min_window_size.height();
#endif
}

// static
std::unique_ptr<WebContentsBasedCanceller> WebContentsBasedCanceller::Create(
    RenderFrameHost* rfh,
    CancelCondition condition,
    gfx::Size min_window_size) {
  if (!rfh) {
    return nullptr;
  }
  // `make_unique` would force the constructor to be public.
  auto canceller = base::WrapUnique(
      new WebContentsBasedCanceller(rfh, condition, min_window_size));
  if (canceller->CanShow()) {
    return canceller;
  }
  return nullptr;
}

WebContentsBasedCanceller::WebContentsBasedCanceller(
    RenderFrameHost* render_frame_host,
    CancelCondition condition,
    gfx::Size min_window_size)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      condition_(condition),
      min_window_size_(min_window_size),
      document_(render_frame_host->GetWeakDocumentPtr()) {}

WebContentsBasedCanceller::~WebContentsBasedCanceller() = default;

bool WebContentsBasedCanceller::CanShow() {
  if (!web_contents()) {
    return false;
  }
  if (condition_ == CancelCondition::kWindowSize) {
    return CanShowForSize();
  }
  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  if (!render_frame_host) {
    return false;
  }
  return CanShowForVisibility(web_contents()->GetVisibility()) &&
         CanShowForRFHActiveState() && CanShowForTabState() && CanShowForSize();
}

bool WebContentsBasedCanceller::CanShowForVisibility(Visibility visibility) {
  return condition_ != CancelCondition::kVisibility ||
         visibility == Visibility::VISIBLE;
}

bool WebContentsBasedCanceller::CanShowForRFHActiveState() {
  if (condition_ == CancelCondition::kWindowSize) {
    return true;
  }
  RenderFrameHost* render_frame_host = document_.AsRenderFrameHostIfValid();
  return render_frame_host && render_frame_host->IsActive();
}

bool WebContentsBasedCanceller::CanShowForTabState() {
  // Within Split View, it is possible for the tab containing a WebContents to
  // be visible but not active. This scenario is considered a cancel condition
  // for kVisibility rather than kActiveState because kActiveState is determined
  // by the RenderFrameHost state, while kVisibility is determined by the
  // WebContents state.
  WebContentsDelegate* web_contents_delegate = web_contents()->GetDelegate();
  return condition_ != CancelCondition::kVisibility || !web_contents_delegate ||
         web_contents_delegate->IsContentsActive(web_contents());
}

bool WebContentsBasedCanceller::CanShowForSize() {
  if (condition_ != CancelCondition::kVisibility &&
      condition_ != CancelCondition::kWindowSize) {
    return true;
  }
  return !IsWindowTooSmall(web_contents(), min_window_size_);
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
  if (cancel_callback_.is_null() ||
      condition_ == CancelCondition::kWindowSize) {
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
  if (cancel_callback_.is_null() ||
      condition_ == CancelCondition::kWindowSize) {
    return;
  }
  if (!document_.AsRenderFrameHostIfValid()) {
    // TODO(https://crbug.com/446032849): Remove this.
    VLOG(1) << "Cancelling";
    std::move(cancel_callback_).Run();
  }
}

void WebContentsBasedCanceller::PrimaryMainFrameWasResized(bool width_changed) {
  VLOG(1) << "Primary main frame was resized";
  if (cancel_callback_.is_null()) {
    return;
  }
  if (!CanShow()) {
    VLOG(1) << "Cancelling due to size change";
    std::move(cancel_callback_).Run();
  }
}

void WebContentsBasedCanceller::FrameSizeChanged(
    RenderFrameHost* render_frame_host,
    const gfx::Size& new_size) {
  VLOG(1) << "Frame size changed: " << new_size.ToString();
  if (cancel_callback_.is_null()) {
    return;
  }
  if ((condition_ == CancelCondition::kWindowSize ||
       render_frame_host == document_.AsRenderFrameHostIfValid()) &&
      !CanShow()) {
    VLOG(1) << "Cancelling due to frame size change";
    std::move(cancel_callback_).Run();
  }
}

}  // namespace content

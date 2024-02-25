// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/display_cutout_host_impl.h"

#include "content/browser/display_cutout/display_cutout_constants.h"
#include "content/browser/display_cutout/safe_area_insets_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

DisplayCutoutHostImpl::DisplayCutoutHostImpl(WebContentsImpl* web_contents)
    : SafeAreaInsetsHost(web_contents) {}

DisplayCutoutHostImpl::~DisplayCutoutHostImpl() = default;

void DisplayCutoutHostImpl::ViewportFitChangedForFrame(
    RenderFrameHost* rfh,
    blink::mojom::ViewportFit value) {
  if (GetValueOrDefault(rfh) == value)
    return;

  values_[rfh] = value;

  // If we are the current |RenderFrameHost| frame then notify
  // WebContentsObservers about the new value.
  if (current_rfh_.get() == rfh)
    web_contents_impl_->NotifyViewportFitChanged(value);
}

void DisplayCutoutHostImpl::DidAcquireFullscreen(RenderFrameHost* rfh) {
  SetCurrentRenderFrameHost(rfh);
}

void DisplayCutoutHostImpl::DidExitFullscreen() {
  SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation is not in the main frame or if we are a same document
  // navigation then we should stop now.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // When Edge To Edge on Android is enabled it needs the messaging sent to
  // Java.
  bool is_not_just_fullscreen =
      base::FeatureList::IsEnabled(features::kDrawCutoutEdgeToEdge);

  // If we finish a main frame navigation and the |WebDisplayMode| is
  // fullscreen then we should make the main frame the current
  // |RenderFrameHost|.  Note that this is probably not correct; we do not check
  // that the navigation completed successfully, nor do we check if the main
  // frame is still IsRenderFrameLive().
  blink::mojom::DisplayMode mode = web_contents_impl_->GetDisplayMode();
  if (is_not_just_fullscreen ||
      mode == blink::mojom::DisplayMode::kFullscreen) {
    SetCurrentRenderFrameHost(web_contents_impl_->GetPrimaryMainFrame());
  }
}

void DisplayCutoutHostImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  values_.erase(rfh);

  // If we were the current |RenderFrameHost| then we should clear that.
  if (current_rfh_.get() == rfh)
    SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::RenderFrameCreated(RenderFrameHost* rfh) {
  ViewportFitChangedForFrame(rfh, blink::mojom::ViewportFit::kAuto);
}

void DisplayCutoutHostImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  insets_ = insets;

  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_.get(), insets);
}

void DisplayCutoutHostImpl::SetCurrentRenderFrameHost(RenderFrameHost* rfh) {
  if (current_rfh_.get() == rfh) {
    if (rfh) {
      // Send an update even when navigating to the same page or doing a reload.
      // When we finish navigation we need to push the Safe Area back to the
      // client to set env() variables for that frame so it can draw correctly.
      SendSafeAreaToFrame(rfh, insets_);
    }
    return;
  }

  // If we had a previous frame then we should clear the insets on that frame.
  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_.get(), gfx::Insets());

  // If the new RenderFrameHost is nullptr we should stop here and notify
  // observers that the new viewport fit is kAuto (the default).
  if (!rfh) {
    current_rfh_ = nullptr;
    web_contents_impl_->NotifyViewportFitChanged(
        blink::mojom::ViewportFit::kAuto);
    return;
  }

  // Update the |current_rfh_| with the new frame.
  current_rfh_ = static_cast<RenderFrameHostImpl*>(rfh)->GetWeakPtr();

  // Send the current safe area to the new frame.
  SendSafeAreaToFrame(rfh, insets_);

  // Notify the WebContentsObservers that the viewport fit value has changed.
  web_contents_impl_->NotifyViewportFitChanged(GetValueOrDefault(rfh));
}

blink::mojom::ViewportFit DisplayCutoutHostImpl::GetValueOrDefault(
    RenderFrameHost* rfh) const {
  auto value = values_.find(rfh);
  if (value != values_.end())
    return value->second;
  return blink::mojom::ViewportFit::kAuto;
}

}  // namespace content

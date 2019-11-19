// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/display_cutout/display_cutout_host_impl.h"

#include "content/browser/display_cutout/display_cutout_constants.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"

namespace content {

DisplayCutoutHostImpl::DisplayCutoutHostImpl(WebContentsImpl* web_contents)
    : bindings_(web_contents, this), web_contents_impl_(web_contents) {}

DisplayCutoutHostImpl::~DisplayCutoutHostImpl() = default;

void DisplayCutoutHostImpl::NotifyViewportFitChanged(
    blink::mojom::ViewportFit value) {
  ViewportFitChangedForFrame(bindings_.GetCurrentTargetFrame(), value);
}

void DisplayCutoutHostImpl::ViewportFitChangedForFrame(
    RenderFrameHost* rfh,
    blink::mojom::ViewportFit value) {
  if (GetValueOrDefault(rfh) == value)
    return;

  values_[rfh] = value;

  // If we are the current |RenderFrameHost| frame then notify
  // WebContentsObservers about the new value.
  if (current_rfh_ == rfh)
    web_contents_impl_->NotifyViewportFitChanged(value);

  MaybeQueueUKMEvent(rfh);
}

void DisplayCutoutHostImpl::DidAcquireFullscreen(RenderFrameHost* rfh) {
  SetCurrentRenderFrameHost(rfh);
}

void DisplayCutoutHostImpl::DidExitFullscreen() {
  SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation is not in the main frame or if we are a same document
  // navigation then we should stop now.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  RecordPendingUKMEvents();
}

void DisplayCutoutHostImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the navigation is not in the main frame or if we are a same document
  // navigation then we should stop now.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // If we finish a main frame navigation and the |WebDisplayMode| is
  // fullscreen then we should make the main frame the current
  // |RenderFrameHost|.
  RenderWidgetHostImpl* rwh =
      web_contents_impl_->GetRenderViewHost()->GetWidget();
  blink::mojom::DisplayMode mode = web_contents_impl_->GetDisplayMode(rwh);
  if (mode == blink::mojom::DisplayMode::kFullscreen)
    SetCurrentRenderFrameHost(web_contents_impl_->GetMainFrame());
}

void DisplayCutoutHostImpl::RenderFrameDeleted(RenderFrameHost* rfh) {
  values_.erase(rfh);

  // If we were the current |RenderFrameHost| then we should clear that.
  if (current_rfh_ == rfh)
    SetCurrentRenderFrameHost(nullptr);
}

void DisplayCutoutHostImpl::RenderFrameCreated(RenderFrameHost* rfh) {
  ViewportFitChangedForFrame(rfh, blink::mojom::ViewportFit::kAuto);
}

void DisplayCutoutHostImpl::WebContentsDestroyed() {
  // Record any pending UKM events that we are waiting to record.
  RecordPendingUKMEvents();
}

void DisplayCutoutHostImpl::SetDisplayCutoutSafeArea(gfx::Insets insets) {
  insets_ = insets;

  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_, insets);

  // If we have a pending UKM event on the top of the stack that is |kAllowed|
  // and we have a |current_rfh_| then we should update that UKM event as it
  // was recorded before we received the safe area.
  if (!pending_ukm_events_.empty() && current_rfh_) {
    PendingUKMEvent& last_entry = pending_ukm_events_.back();
    if (last_entry.ignored_reason == DisplayCutoutIgnoredReason::kAllowed)
      last_entry.safe_areas_present = GetSafeAreasPresentUKMValue();
  }
}

void DisplayCutoutHostImpl::SetCurrentRenderFrameHost(RenderFrameHost* rfh) {
  if (current_rfh_ == rfh)
    return;

  // If we had a previous frame then we should clear the insets on that frame.
  if (current_rfh_)
    SendSafeAreaToFrame(current_rfh_, gfx::Insets());

  // Update the |current_rfh_| with the new frame.
  current_rfh_ = rfh;

  // If the new RenderFrameHost is nullptr we should stop here and notify
  // observers that the new viewport fit is kAuto (the default).
  if (!rfh) {
    web_contents_impl_->NotifyViewportFitChanged(
        blink::mojom::ViewportFit::kAuto);
    return;
  }

  // Record a UKM event for the new frame.
  MaybeQueueUKMEvent(current_rfh_);

  // Send the current safe area to the new frame.
  SendSafeAreaToFrame(rfh, insets_);

  // Notify the WebContentsObservers that the viewport fit value has changed.
  web_contents_impl_->NotifyViewportFitChanged(GetValueOrDefault(rfh));
}

void DisplayCutoutHostImpl::SendSafeAreaToFrame(RenderFrameHost* rfh,
                                                gfx::Insets insets) {
  blink::AssociatedInterfaceProvider* provider =
      rfh->GetRemoteAssociatedInterfaces();
  if (!provider)
    return;

  mojo::AssociatedRemote<blink::mojom::DisplayCutoutClient> client;
  provider->GetInterface(client.BindNewEndpointAndPassReceiver());
  client->SetSafeArea(blink::mojom::DisplayCutoutSafeArea::New(
      insets.top(), insets.left(), insets.bottom(), insets.right()));
}

blink::mojom::ViewportFit DisplayCutoutHostImpl::GetValueOrDefault(
    RenderFrameHost* rfh) const {
  auto value = values_.find(rfh);
  if (value != values_.end())
    return value->second;
  return blink::mojom::ViewportFit::kAuto;
}

void DisplayCutoutHostImpl::MaybeQueueUKMEvent(RenderFrameHost* frame) {
  if (!frame)
    return;

  // Get the current applied ViewportFit and the ViewportFit value supplied by
  // |frame|. If the |supplied_value| is kAuto then we will not record the
  // event since it is the default.
  blink::mojom::ViewportFit supplied_value = GetValueOrDefault(frame);
  if (supplied_value == blink::mojom::ViewportFit::kAuto)
    return;
  blink::mojom::ViewportFit applied_value = GetValueOrDefault(current_rfh_);

  // Set the reason why this frame is not the current frame.
  int ignored_reason = DisplayCutoutIgnoredReason::kAllowed;
  if (current_rfh_ != frame) {
    ignored_reason =
        current_rfh_ == nullptr
            ? DisplayCutoutIgnoredReason::kWebContentsNotFullscreen
            : DisplayCutoutIgnoredReason::kFrameNotCurrentFullscreen;
  }

  // Adds the UKM event to the list of pending events.
  PendingUKMEvent pending_event;
  pending_event.is_main_frame = !frame->GetParent();
  pending_event.applied_value = applied_value;
  pending_event.supplied_value = supplied_value;
  pending_event.ignored_reason = ignored_reason;
  if (ignored_reason == DisplayCutoutIgnoredReason::kAllowed)
    pending_event.safe_areas_present = GetSafeAreasPresentUKMValue();
  pending_ukm_events_.push_back(pending_event);
}

void DisplayCutoutHostImpl::RecordPendingUKMEvents() {
  for (const auto& event : pending_ukm_events_) {
    ukm::builders::Layout_DisplayCutout_StateChanged builder(
        web_contents_impl_->GetUkmSourceIdForLastCommittedSource());
    builder.SetIsMainFrame(event.is_main_frame);
    builder.SetViewportFit_Applied(static_cast<int>(event.applied_value));
    builder.SetViewportFit_Supplied(static_cast<int>(event.supplied_value));
    builder.SetViewportFit_IgnoredReason(event.ignored_reason);
    builder.SetSafeAreasPresent(event.safe_areas_present);
    builder.Record(ukm::UkmRecorder::Get());
  }

  pending_ukm_events_.clear();
}

int DisplayCutoutHostImpl::GetSafeAreasPresentUKMValue() const {
  int flags = 0;
  flags |= insets_.top() ? DisplayCutoutSafeArea::kTop : 0;
  flags |= insets_.left() ? DisplayCutoutSafeArea::kLeft : 0;
  flags |= insets_.bottom() ? DisplayCutoutSafeArea::kBottom : 0;
  flags |= insets_.right() ? DisplayCutoutSafeArea::kRight : 0;
  return flags;
}

}  // namespace content

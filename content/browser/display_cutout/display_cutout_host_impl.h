// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_
#define CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_receiver_set.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"

namespace content {

class DisplayCutoutHostImpl : public blink::mojom::DisplayCutoutHost {
 public:
  explicit DisplayCutoutHostImpl(WebContentsImpl*);
  ~DisplayCutoutHostImpl() override;

  // blink::mojom::DisplayCutoutHost
  void NotifyViewportFitChanged(blink::mojom::ViewportFit value) override;

  // Stores the updated viewport fit value for a |frame| and notifies observers
  // if it has changed.
  void ViewportFitChangedForFrame(RenderFrameHost* rfh,
                                  blink::mojom::ViewportFit value);

  // Called by WebContents when various events occur.
  void DidAcquireFullscreen(RenderFrameHost* rfh);
  void DidExitFullscreen();
  void DidFinishNavigation(NavigationHandle* navigation_handle);
  void RenderFrameDeleted(RenderFrameHost* rfh);
  void RenderFrameCreated(RenderFrameHost* rfh);
  void WebContentsDestroyed();

  // Updates the safe area insets on the current frame.
  void SetDisplayCutoutSafeArea(gfx::Insets insets);

 private:
  // Stores the data for a pending UKM event.
  struct PendingUKMEvent {
    ukm::SourceId source_id;
    bool is_main_frame;
    blink::mojom::ViewportFit applied_value;
    blink::mojom::ViewportFit supplied_value;
    int ignored_reason;
    int safe_areas_present = 0;
  };

  // Set the current |RenderFrameHost| that should have control over the
  // viewport fit value and we should set safe area insets on.
  void SetCurrentRenderFrameHost(RenderFrameHost* rfh);

  // Send the safe area insets to a |RenderFrameHost|.
  void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets);

  // Get the stored viewport fit value for a frame or kAuto if there is no
  // stored value.
  blink::mojom::ViewportFit GetValueOrDefault(RenderFrameHost* rfh) const;

  // Builds and records a Layout.DisplayCutout.StateChanged UKM event for the
  // provided |frame|. The event will be added to the list of pending events.
  void MaybeQueueUKMEvent(RenderFrameHost* frame);

  // Records any UKM events that we have not recorded yet.
  void RecordPendingUKMEvents();

  // Gets the integer value of the current safe areas for recording to UKM.
  int GetSafeAreasPresentUKMValue() const;

  // Stores pending UKM events.
  std::list<PendingUKMEvent> pending_ukm_events_;

  // Stores the current safe area insets.
  gfx::Insets insets_;

  // Stores the current |RenderFrameHost| that has the applied safe area insets
  // and is controlling the viewport fit value.
  RenderFrameHost* current_rfh_ = nullptr;

  // Stores a map of RenderFrameHosts and their current viewport fit values.
  std::map<RenderFrameHost*, blink::mojom::ViewportFit> values_;

  // Holds WebContents associated mojo receivers.
  WebContentsFrameReceiverSet<blink::mojom::DisplayCutoutHost> receivers_;

  // Weak pointer to the owning |WebContentsImpl| instance.
  WebContentsImpl* web_contents_impl_;

  DISALLOW_COPY_AND_ASSIGN(DisplayCutoutHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_

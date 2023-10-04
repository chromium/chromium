// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_
#define CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "ui/gfx/geometry/insets.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

class DisplayCutoutHostImpl : public blink::mojom::DisplayCutoutHost {
 public:
  explicit DisplayCutoutHostImpl(WebContentsImpl*);

  DisplayCutoutHostImpl(const DisplayCutoutHostImpl&) = delete;
  DisplayCutoutHostImpl& operator=(const DisplayCutoutHostImpl&) = delete;

  ~DisplayCutoutHostImpl() override;

  // Binds a new receiver for the specified frame.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver,
      RenderFrameHost* rfh);

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

  // Updates the safe area insets on the current frame.
  void SetDisplayCutoutSafeArea(gfx::Insets insets);

 private:
  // Set the current |RenderFrameHost| that should have control over the
  // viewport fit value and we should set safe area insets on.
  void SetCurrentRenderFrameHost(RenderFrameHost* rfh);

  // Send the safe area insets to a |RenderFrameHost|.
  void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets);

  // Get the stored viewport fit value for a frame or kAuto if there is no
  // stored value.
  blink::mojom::ViewportFit GetValueOrDefault(RenderFrameHost* rfh) const;

  // Stores the current safe area insets.
  gfx::Insets insets_;

  // Stores the current |RenderFrameHost| that has the applied safe area insets
  // and is controlling the viewport fit value. This value is different than
  // `WebContentsImpl::current_fullscreen_frame_id_` because it also considers
  // browser side driven fullscreen mode, not just renderer side requested
  // frames.
  base::WeakPtr<RenderFrameHostImpl> current_rfh_;

  // Stores a map of RenderFrameHosts and their current viewport fit values.
  std::map<RenderFrameHost*, blink::mojom::ViewportFit> values_;

  // Holds WebContents associated mojo receivers.
  RenderFrameHostReceiverSet<blink::mojom::DisplayCutoutHost> receivers_;

  // Weak pointer to the owning |WebContentsImpl| instance.
  raw_ptr<WebContentsImpl> web_contents_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_HOST_IMPL_H_

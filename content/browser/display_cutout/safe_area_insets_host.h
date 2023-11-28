// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_H_
#define CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_H_

#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "ui/gfx/geometry/insets.h"

namespace content {

class RenderFrameHost;
class WebContentsImpl;

// This is an Abstract base class for a Host that handles Safe Area Insets such
// as the Notch (Display Cutout) and the Android Edge To Edge feature.
class CONTENT_EXPORT SafeAreaInsetsHost
    : public blink::mojom::DisplayCutoutHost {
 public:
  static std::unique_ptr<SafeAreaInsetsHost> Create(WebContentsImpl*);

  SafeAreaInsetsHost(const SafeAreaInsetsHost&) = delete;
  SafeAreaInsetsHost& operator=(const SafeAreaInsetsHost&) = delete;

  ~SafeAreaInsetsHost() override;

  // Binds a new receiver for the specified frame.
  void BindReceiver(
      mojo::PendingAssociatedReceiver<blink::mojom::DisplayCutoutHost> receiver,
      RenderFrameHost* rfh);

  // blink::mojom::DisplayCutoutHost
  void NotifyViewportFitChanged(blink::mojom::ViewportFit value) final;

  // Called by WebContents when various events occur.
  virtual void DidAcquireFullscreen(RenderFrameHost* rfh) = 0;
  virtual void DidExitFullscreen() = 0;
  virtual void DidFinishNavigation(NavigationHandle* navigation_handle) = 0;
  virtual void RenderFrameDeleted(RenderFrameHost* rfh) = 0;
  virtual void RenderFrameCreated(RenderFrameHost* rfh) = 0;

  // Updates the safe area insets on the current frame.
  virtual void SetDisplayCutoutSafeArea(gfx::Insets insets) = 0;

 protected:
  explicit SafeAreaInsetsHost(WebContentsImpl*);

  // Stores the updated viewport fit value for a |frame| and notifies observers
  // if it has changed.
  virtual void ViewportFitChangedForFrame(RenderFrameHost* rfh,
                                          blink::mojom::ViewportFit value) = 0;

  // Send the safe area insets to Blink through a |RenderFrameHost|.
  // Protected and virtual for testing only.
  virtual void SendSafeAreaToFrame(RenderFrameHost* rfh, gfx::Insets insets);

  // Weak pointer to the owning |WebContentsImpl| instance.
  raw_ptr<WebContentsImpl> web_contents_impl_;

 private:
  // Holds WebContents associated mojo receivers.
  RenderFrameHostReceiverSet<blink::mojom::DisplayCutoutHost> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_H_

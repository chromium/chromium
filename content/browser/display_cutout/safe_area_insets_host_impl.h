// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_IMPL_H_
#define CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/display_cutout/safe_area_insets_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/mojom/page/display_cutout.mojom.h"
#include "ui/gfx/geometry/insets.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

// Handles changes to Safe Area Insets (SAI) by monitoring
// navigations within a `WebContents` and hosting a connection
// to Blink. See the base class `SafeAreaInsetsHost` for context.

// Tracks the viewport-fit value associated with each Blink Document
// as `WebContents` are updated through navigation etc. As of 2023 each
// RenderFrameHost may correspond to one or more documents during in-site
// navigation, so this class maps the viewport-fit value of a document to the
// associated RFH and updates the owning `WebContents` when it changes.
// Note that subframes may acquire fullscreen so the viewport-fit from that
// frame may change the insets.
//
// This class ensures there will be only one frame that receives the current
// SAI, with this rule:
//  * When a fullscreen frame exists, the fullscreen frame will take the SAI
//  * When no fullscreen frame exists, the primary main frame will take the SAI
//  * When the frame that takes SAI changes, the SAI in the previous frame will
//    be reset.
class CONTENT_EXPORT SafeAreaInsetsHostImpl : public SafeAreaInsetsHost {
 public:
  explicit SafeAreaInsetsHostImpl(WebContentsImpl*);

  SafeAreaInsetsHostImpl(const SafeAreaInsetsHostImpl&) = delete;
  SafeAreaInsetsHostImpl& operator=(const SafeAreaInsetsHostImpl&) = delete;

  ~SafeAreaInsetsHostImpl() override;

  void DidAcquireFullscreen(RenderFrameHost* rfh) override;
  void DidExitFullscreen() override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void RenderFrameDeleted(RenderFrameHost* rfh) override {}
  void RenderFrameCreated(RenderFrameHost* rfh) override {}

  void SetDisplayCutoutSafeArea(gfx::Insets insets) override;

 protected:
  // SafeAreaInsetsHost override.
  void ViewportFitChangedForFrame(RenderFrameHost* rfh,
                                  blink::mojom::ViewportFit value) override;

  // Get the stored viewport fit value for a frame or kAuto if there is no
  // stored value.
  // Protected for testing only.
  blink::mojom::ViewportFit GetValueOrDefault(RenderFrameHost* rfh) const;

  // Sets the stored viewport fit value for a frame, deleting the UserData
  // if it's no longer needed.
  // Protected for testing only.
  void SetViewportFitValue(RenderFrameHost* rfh,
                           blink::mojom::ViewportFit value);

 private:
  friend class TestSafeAreaInsetsHostImpl;

  // Checks if the active `RenderFrameHost` has changed, and notifies
  // Blink about the current safe area, and WebContents observers if needed.
  void MaybeActiveRenderFrameHostChanged();

  // Returns the current active `RenderFrameHost`: the current RFH or the
  // fullscreen RFH when in Fullscreen mode. May return `nullptr` during
  // startup.
  RenderFrameHostImpl* ActiveRenderFrameHost();

  // Stores the current primary main `RenderFrameHost`. Never `nullptr` except
  // during startup.
  base::WeakPtr<RenderFrameHostImpl> current_rfh_;

  // Stores the `RenderFrameHost` being displayed in fullscreen, and is
  // `nullptr` when not in fullscreen.
  base::WeakPtr<RenderFrameHostImpl> fullscreen_rfh_;

  // Stores the current active `RenderFrameHost` that received the safe area
  // insets. This could be either the `current_rfh`, or `fullscreen_rfh_`
  // if in fullscreen mode. Caching this to keep track when the active
  // `RenderFrameHost` changes. Should only be accessed in
  // `MaybeActiveRenderFrameHostChanged()`; other code should use
  // `ActiveRenderFrameHost()` instead.
  base::WeakPtr<RenderFrameHostImpl> active_rfh_;

  // Stores the viewport-fit value that's active for this WebContents.
  blink::mojom::ViewportFit active_value_ = blink::mojom::ViewportFit::kAuto;

  // The current insets.
  gfx::Insets insets_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DISPLAY_CUTOUT_SAFE_AREA_INSETS_HOST_IMPL_H_

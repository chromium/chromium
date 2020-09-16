/*
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_VIEW_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_VIEW_H_

#include "base/time/time.h"
#include "third_party/blink/public/common/page/web_drag_operation.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "third_party/blink/public/mojom/page/page.mojom-shared.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/mojom/widget/screen_orientation.mojom-shared.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class PaintCanvas;
struct BrowserControlsParams;
}

namespace gfx {
class ColorSpace;
class Point;
class PointF;
class Rect;
class SizeF;
}

namespace blink {
class PageScheduler;
class WebFrame;
class WebFrameWidget;
class WebHitTestResult;
class WebLocalFrame;
class WebPageImportanceSignals;
class WebPagePopup;
class WebPrerendererClient;
class WebRemoteFrame;
class WebSettings;
class WebString;
class WebViewClient;
class WebWidget;
struct DeviceEmulationParams;
struct WebRect;
struct WebSize;
struct WebWindowFeatures;

class WebView {
 public:
  BLINK_EXPORT static const double kTextSizeMultiplierRatio;
  BLINK_EXPORT static const double kMinTextSizeMultiplier;
  BLINK_EXPORT static const double kMaxTextSizeMultiplier;

  enum StyleInjectionTarget {
    kInjectStyleInAllFrames,
    kInjectStyleInTopFrameOnly
  };

  // Initialization ------------------------------------------------------

  // Creates a WebView that is NOT yet initialized. To complete initialization,
  // call WebLocalFrame::CreateMainFrame() or WebRemoteFrame::CreateMainFrame()
  // as appropriate. It is legal to modify settings before completing
  // initialization.
  //
  // clients may be null, but should both be null or not together.
  // |is_hidden| defines the initial visibility of the page.
  // [is_inside_portal] defines whether the page is inside_portal.
  // |compositing_enabled| dictates whether accelerated compositing should be
  // enabled for the page. It must be false if no clients are provided, or if a
  // LayerTreeView will not be set for the WebWidget.
  // TODO(danakj): This field should go away as WebWidgets always composite
  // their output.
  // |page_handle| is only set for views that are part of a WebContents' frame
  // tree.
  // TODO(yuzus): Remove |is_hidden| and start using |PageVisibilityState|.
  BLINK_EXPORT static WebView* Create(
      WebViewClient*,
      bool is_hidden,
      bool is_inside_portal,
      bool compositing_enabled,
      WebView* opener,
      CrossVariantMojoAssociatedReceiver<mojom::PageBroadcastInterfaceBase>
          page_handle);

  // Destroys the WebView.
  virtual void Close() = 0;

  // Sets whether the WebView is focused.
  virtual void SetFocus(bool enable) = 0;

  // Called to inform WebViewImpl that a local main frame has been attached.
  // After this call MainFrameImpl() will return a valid frame until it is
  // detached.
  virtual void DidAttachLocalMainFrame() = 0;

  // Called while the main LocalFrame is being detached. The MainFrameImpl() is
  // still valid until after this method is called.
  virtual void DidDetachLocalMainFrame() = 0;

  // Called to inform WebViewImpl that a remote main frame has been attached.
  virtual void DidAttachRemoteMainFrame() = 0;

  // Called to inform WebViewImpl that a remote main frame has been detached.
  virtual void DidDetachRemoteMainFrame() = 0;

  // Initializes the various client interfaces.
  virtual void SetPrerendererClient(WebPrerendererClient*) = 0;

  // Called when some JS code has instructed the window associated to the main
  // frame to close, which will result in a request to the browser to close the
  // RenderWidget associated to it.
  virtual void CloseWindowSoon() = 0;

  // Options -------------------------------------------------------------

  // The returned pointer is valid for the lifetime of the WebView.
  virtual WebSettings* GetSettings() = 0;

  // Corresponds to the encoding of the main frame.  Setting the page
  // encoding may cause the main frame to reload.
  virtual WebString PageEncoding() const = 0;

  // Controls whether pressing Tab key advances focus to links.
  virtual bool TabsToLinks() const = 0;
  virtual void SetTabsToLinks(bool) = 0;

  // Method that controls whether pressing Tab key cycles through page
  // elements or inserts a '\t' char in the focused text area.
  virtual bool TabKeyCyclesThroughElements() const = 0;
  virtual void SetTabKeyCyclesThroughElements(bool) = 0;

  // Controls the WebView's active state, which may affect the rendering
  // of elements on the page (i.e., tinting of input elements).
  virtual bool IsActive() const = 0;
  virtual void SetIsActive(bool) = 0;

  // Allows setting the state of the various bars exposed via BarProp
  // properties on the window object. The size related fields of
  // WebWindowFeatures are ignored.
  virtual void SetWindowFeatures(const WebWindowFeatures&) = 0;

  // Marks the WebView as being opened by a DOM call. This is relevant
  // for whether window.close() may be called.
  virtual void SetOpenedByDOM() = 0;

  // Frames --------------------------------------------------------------

  virtual WebFrame* MainFrame() = 0;

  // Focus ---------------------------------------------------------------

  virtual WebLocalFrame* FocusedFrame() = 0;
  virtual void SetFocusedFrame(WebFrame*) = 0;

  // Smooth scroll the root layer to |targetX|, |targetY| in |duration|.
  virtual void SmoothScroll(int target_x,
                            int target_y,
                            base::TimeDelta duration) {}

  // Advance the focus of the WebView forward to the next element or to the
  // previous element in the tab sequence (if reverse is true).
  virtual void AdvanceFocus(bool reverse) {}

  // Changes the zoom and scroll for zooming into an editable element
  // with bounds |element_bounds_in_document| and caret bounds
  // |caret_bounds_in_document|.
  virtual void ZoomAndScrollToFocusedEditableElementRect(
      const WebRect& element_bounds_in_document,
      const WebRect& caret_bounds_in_document,
      bool zoom_into_legible_scale) = 0;

  // Zoom ----------------------------------------------------------------

  // Returns the current zoom level.  0 is "original size", and each increment
  // above or below represents zooming 20% larger or smaller to default limits
  // of 300% and 50% of original size, respectively.  Only plugins use
  // non whole-numbers, since they might choose to have specific zoom level so
  // that fixed-width content is fit-to-page-width, for example.
  virtual double ZoomLevel() = 0;

  // Changes the zoom level to the specified level, clamping at the limits
  // noted above, and returns the current zoom level after applying the
  // change.
  virtual double SetZoomLevel(double) = 0;

  // Gets the scale factor of the page, where 1.0 is the normal size, > 1.0
  // is scaled up, < 1.0 is scaled down.
  virtual float PageScaleFactor() const = 0;

  // Scales the page without affecting layout by using the visual viewport.
  virtual void SetPageScaleFactor(float) = 0;

  // Minimum and Maximum as computed as a combination of default, page defined,
  // UA, etc. constraints.
  virtual float MinimumPageScaleFactor() const = 0;
  virtual float MaximumPageScaleFactor() const = 0;

  // Sets the offset of the visual viewport within the main frame, in
  // fractional CSS pixels. The offset will be clamped so the visual viewport
  // stays within the frame's bounds.
  virtual void SetVisualViewportOffset(const gfx::PointF&) = 0;

  // Gets the visual viewport's current offset within the page's main frame,
  // in fractional CSS pixels.
  virtual gfx::PointF VisualViewportOffset() const = 0;

  // Get the visual viewport's size in CSS pixels.
  virtual gfx::SizeF VisualViewportSize() const = 0;

  // Sets the default minimum, and maximum page scale. These will be overridden
  // by the page or by the overrides below if they are set.
  virtual void SetDefaultPageScaleLimits(float min_scale, float max_scale) = 0;

  // Sets the initial page scale to the given factor. This scale setting
  // overrides
  // page scale set in the page's viewport meta tag.
  virtual void SetInitialPageScaleOverride(float) = 0;

  // Sets the maximum page scale considered to be legible. Automatic zooms (e.g,
  // double-tap or find in page) will have the page scale limited to this value
  // times the font scale factor. Manual pinch zoom will not be affected by this
  // limit.
  virtual void SetMaximumLegibleScale(float) = 0;

  // Reset any saved values for the scroll and scale state.
  virtual void ResetScrollAndScaleState() = 0;

  // Prevent the web page from setting min/max scale via the viewport meta
  // tag. This is an accessibility feature that lets folks zoom in to web
  // pages even if the web page tries to block scaling.
  virtual void SetIgnoreViewportTagScaleLimits(bool) = 0;

  // Returns the "preferred" contents size, defined as the preferred minimum
  // width of the main document's contents and the minimum height required to
  // display the main document without scrollbars. If the document is in quirks
  // mode (does not have <!doctype html>), the height will stretch to fill the
  // viewport. The returned size has the page zoom factor applied. The lifecycle
  // must be updated to at least layout before calling (see: |UpdateLifecycle|).
  //
  // This may only be called when there is a local main frame attached to this
  // WebView.
  virtual WebSize ContentsPreferredMinimumSize() = 0;

  // Check whether the preferred size has changed. This should only be called
  // with up-to-date layout.
  virtual void UpdatePreferredSize() = 0;

  // Indicates that view's preferred size changes will be sent to the browser.
  virtual void EnablePreferredSizeChangedMode() = 0;

  // Asks the browser process to activate this web view.
  virtual void Focus() = 0;

  // Sets the ratio as computed by computePageScaleConstraints.
  // TODO(oshima): Remove this once the device scale factor implementation is
  // fully migrated to use zooming mechanism.
  virtual void SetDeviceScaleFactor(float) = 0;

  // Sets the additional zoom factor used for device scale factor. This is used
  // to scale the content by the device scale factor, without affecting zoom
  // level.
  virtual void SetZoomFactorForDeviceScaleFactor(float) = 0;

  virtual float ZoomFactorForDeviceScaleFactor() = 0;

  // This method is used for testing.
  // Resize the view at the same time as changing the state of the top
  // controls. If |browser_controls_shrink_layout| is true, the embedder shrunk
  // the WebView size by the browser controls height.
  virtual void ResizeWithBrowserControls(
      const WebSize& main_frame_widget_size,
      float top_controls_height,
      float bottom_controls_height,
      bool browser_controls_shrink_layout) = 0;
  // This method is used for testing.
  // Resizes the unscaled (page scale = 1.0) visual viewport. Normally the
  // unscaled visual viewport is the same size as the main frame. The passed
  // size becomes the size of the viewport when page scale = 1. This
  // is used to shrink the visible viewport to allow things like the ChromeOS
  // virtual keyboard to overlay over content but allow scrolling it into view.
  virtual void ResizeVisualViewport(const WebSize&) = 0;

  // Same as ResizeWithBrowserControls(const WebSize&,float,float,bool), but
  // includes all browser controls params such as the min heights.
  virtual void ResizeWithBrowserControls(
      const WebSize& main_frame_widget_size,
      const WebSize& visible_viewport_size,
      cc::BrowserControlsParams browser_controls_params) = 0;

  // Same as ResizeWithBrowserControls, but keeps the same BrowserControl
  // settings.
  virtual void Resize(const WebSize&) = 0;

  virtual WebSize GetSize() = 0;

  // Override the screen orientation override.
  virtual void SetScreenOrientationOverrideForTesting(
      base::Optional<blink::mojom::ScreenOrientation> orientation) = 0;

  // Auto-Resize -----------------------------------------------------------

  // Return the state of the auto resize mode.
  virtual bool AutoResizeMode() = 0;

  // Enable auto resize.
  virtual void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                          const gfx::Size& max_size) = 0;

  // Disable auto resize.
  virtual void DisableAutoResizeForTesting(const gfx::Size& new_size) = 0;

  // Data exchange -------------------------------------------------------

  // Do a hit test equivalent to what would be done for a GestureTap event
  // that has width/height corresponding to the supplied |tapArea|.
  virtual WebHitTestResult HitTestResultForTap(const gfx::Point& tap_point,
                                               const WebSize& tap_area) = 0;

  // Support for resource loading initiated by plugins -------------------

  // Returns next unused request identifier which is unique within the
  // parent Page.
  virtual uint64_t CreateUniqueIdentifierForRequest() = 0;

  // Developer tools -----------------------------------------------------

  // Enables device emulation as specified in params.
  virtual void EnableDeviceEmulation(const DeviceEmulationParams&) = 0;

  // Cancel emulation started via |enableDeviceEmulation| call.
  virtual void DisableDeviceEmulation() = 0;

  // Context menu --------------------------------------------------------

  virtual void PerformCustomContextMenuAction(unsigned action) = 0;

  // Notify that context menu has been closed.
  virtual void DidCloseContextMenu() = 0;

  // Popup menu ----------------------------------------------------------

  // Sets whether select popup menus should be rendered by the browser.
  BLINK_EXPORT static void SetUseExternalPopupMenus(bool);

  // Cancels and hides the current popup (datetime, select...) if any.
  virtual void CancelPagePopup() = 0;

  // Returns the current popup if any.
  virtual WebPagePopup* GetPagePopup() const = 0;

  // Visited link state --------------------------------------------------

  // Tells all WebView instances to update the visited link state for the
  // specified hash.
  BLINK_EXPORT static void UpdateVisitedLinkState(uint64_t hash);

  // Tells all WebView instances to update the visited state for all
  // their links. Use invalidateVisitedLinkHashes to inform that the visitedlink
  // table was changed and the salt was changed too. And all cached visitedlink
  // hashes need to be recalculated.
  BLINK_EXPORT static void ResetVisitedLinkState(
      bool invalidate_visited_link_hashes);

  // Custom colors -------------------------------------------------------

  // Sets the default background color when the page has not loaded enough to
  // know a background colour. This can be overridden by the methods below as
  // well.
  virtual void SetBaseBackgroundColor(SkColor) {}

  virtual void SetDeviceColorSpaceForTesting(
      const gfx::ColorSpace& color_space) = 0;

  // Scheduling -----------------------------------------------------------

  virtual PageScheduler* Scheduler() const = 0;

  // Visibility -----------------------------------------------------------

  // Sets the visibility of the WebView.
  virtual void SetVisibilityState(mojom::PageVisibilityState visibility_state,
                                  bool is_initial_state) = 0;
  virtual mojom::PageVisibilityState GetVisibilityState() = 0;

  // PageLifecycleState ----------------------------------------------------

  // Sets the |visibility| and |pagehide_dispatch| properties for the
  // PageLifecycleState of this page from a new page's commit. Should only be
  // called from a main-frame same-site navigation where we did a proactive
  // BrowsingInstance swap and we're reusing the old page's process.
  // Note that unlike SetPageLifecycleState in PageBroadcast/WebViewImpl, we
  // don't need to pass a callback here to notify the browser site that the
  // PageLifecycleState has been successfully updated.
  // TODO(rakina): When it's possible to pass PageLifecycleState here, pass
  // PageLifecycleState instead.
  virtual void SetPageLifecycleStateFromNewPageCommit(
      mojom::PageVisibilityState visibility,
      mojom::PagehideDispatch pagehide_dispatch) = 0;

  // Page Importance Signals ----------------------------------------------

  virtual WebPageImportanceSignals* PageImportanceSignals() { return nullptr; }

  // i18n -----------------------------------------------------------------

  // Inform the WebView that the accept languages have changed.
  // If the WebView wants to get the accept languages value, it will have
  // to call the WebViewClient::acceptLanguages().
  virtual void AcceptLanguagesChanged() = 0;

  // Lifecycle state ------------------------------------------------------

  // Freezes or unfreezes the page and all the local frames.
  virtual void SetPageFrozen(bool frozen) = 0;

  // Autoplay configuration -----------------------------------------------

  // Sets the autoplay flags for this webview's page.
  // The valid flags are defined in
  // third_party/blink/public/platform/autoplay.mojom
  virtual void AddAutoplayFlags(int32_t flags) = 0;
  virtual void ClearAutoplayFlags() = 0;
  virtual int32_t AutoplayFlagsForTest() = 0;
  virtual WebSize GetPreferredSizeForTest() = 0;

  // Non-composited support -----------------------------------------------

  // Called to paint the rectangular region within the WebView's main frame
  // onto the specified canvas at (viewport.x, viewport.y). This is to provide
  // support for non-composited WebViews, and is used to paint into a
  // PaintCanvas being supplied by another (composited) WebView.
  //
  // Before calling PaintContent(), the caller must ensure the lifecycle of the
  // widget's frame is clean by calling
  // UpdateLifecycle(WebLifecycleUpdate::All). It is okay to call paint multiple
  // times once the lifecycle is clean, assuming no other changes are made to
  // the WebWidget (e.g., once events are processed, it should be assumed that
  // another call to UpdateLifecycle is warranted before painting again). Paints
  // starting from the main LayoutView's property tree state, thus ignoring any
  // transient transormations (e.g. pinch-zoom, dev tools emulation, etc.).
  //
  // The painting will be performed without applying the DevicePixelRatio as
  // scaling is expected to already be applied to the PaintCanvas by the
  // composited WebView which supplied the PaintCanvas. The canvas state may
  // be modified and should be saved before calling this method and restored
  // after.
  virtual void PaintContent(cc::PaintCanvas*, const gfx::Rect& viewport) = 0;

  // Web preferences ---------------------------------------------------

  // Applies blink related preferences to this view.
  BLINK_EXPORT static void ApplyWebPreferences(
      const web_pref::WebPreferences& prefs,
      WebView* web_view);

  virtual void SetWebPreferences(
      const web_pref::WebPreferences& preferences) = 0;
  virtual const web_pref::WebPreferences& GetWebPreferences() = 0;

  // TODO(lfg): Remove this once the refactor of WebView/WebWidget is
  // completed.
  virtual WebFrameWidget* MainFrameWidget() = 0;

  // Portals --------------------------------------------------------------

 protected:
  ~WebView() = default;
};

}  // namespace blink

#endif

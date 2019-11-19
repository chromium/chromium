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
#include "third_party/blink/public/common/page/page_visibility_state.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class PaintCanvas;
}

namespace gfx {
class Point;
class Rect;
}

namespace blink {
class PageScheduler;
class WebFrame;
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
struct PluginAction;
struct WebDeviceEmulationParams;
struct WebFloatPoint;
struct WebFloatSize;
struct WebRect;
struct WebSize;
struct WebTextAutosizerPageInfo;
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
  // |compositing_enabled| dictates whether accelerated compositing should be
  // enabled for the page. It must be false if no clients are provided, or if a
  // LayerTreeView will not be set for the WebWidget.
  // TODO(danakj): This field should go away as WebWidgets always composite
  // their output.
  BLINK_EXPORT static WebView* Create(WebViewClient*,
                                      bool is_hidden,
                                      bool compositing_enabled,
                                      WebView* opener);

  // Destroys the WebView.
  virtual void Close() = 0;

  // Sets whether the WebView is focused.
  virtual void SetFocus(bool enable) = 0;

  // Called to inform WebViewImpl that a local main frame has been attached.
  // After this call MainFrameImpl() will return a valid frame until it is
  // detached.
  virtual void DidAttachLocalMainFrame() = 0;

  // Initializes the various client interfaces.
  virtual void SetPrerendererClient(WebPrerendererClient*) = 0;

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

  // Allows disabling domain relaxation.
  virtual void SetDomainRelaxationForbidden(bool, const WebString& scheme) = 0;

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

  // Sets the provided frame as focused and fires blur/focus events on any
  // currently focused elements in old/new focused documents.  Note that this
  // is different from setFocusedFrame, which does not fire events on focused
  // elements.
  virtual void FocusDocumentView(WebFrame*) = 0;

  // Focus the first (last if reverse is true) focusable node.
  virtual void SetInitialFocus(bool reverse) = 0;

  // Clears the focused element (and selection if a text field is focused)
  // to ensure that a text field on the page is not eating keystrokes we
  // send it.
  virtual void ClearFocusedElement() = 0;

  // Smooth scroll the root layer to |targetX|, |targetY| in |duration|.
  virtual void SmoothScroll(int target_x,
                            int target_y,
                            base::TimeDelta duration) {}

  // Advance the focus of the WebView forward to the next element or to the
  // previous element in the tab sequence (if reverse is true).
  virtual void AdvanceFocus(bool reverse) {}

  // Advance the focus from the frame |from| to the next in sequence
  // (determined by WebFocusType) focusable element in frame |to|. Used when
  // focus needs to advance to/from a cross-process frame.
  virtual void AdvanceFocusAcrossFrames(WebFocusType,
                                        WebRemoteFrame* from,
                                        WebLocalFrame* to) {}

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

  // Returns the current text zoom factor, where 1.0 is the normal size, > 1.0
  // is scaled up and < 1.0 is scaled down.
  virtual float TextZoomFactor() = 0;

  // Scales the text in the page by a factor of textZoomFactor.
  // Note: this has no effect on plugins.
  virtual float SetTextZoomFactor(float) = 0;

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
  virtual void SetVisualViewportOffset(const WebFloatPoint&) = 0;

  // Gets the visual viewport's current offset within the page's main frame,
  // in fractional CSS pixels.
  virtual WebFloatPoint VisualViewportOffset() const = 0;

  // Get the visual viewport's size in CSS pixels.
  virtual WebFloatSize VisualViewportSize() const = 0;

  // Resizes the unscaled (page scale = 1.0) visual viewport. Normally the
  // unscaled visual viewport is the same size as the main frame. The passed
  // size becomes the size of the viewport when page scale = 1. This
  // is used to shrink the visible viewport to allow things like the ChromeOS
  // virtual keyboard to overlay over content but allow scrolling it into view.
  virtual void ResizeVisualViewport(const WebSize&) = 0;

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

  // Requests a page-scale animation based on the specified point/rect.
  virtual void AnimateDoubleTapZoom(const gfx::Point&, const WebRect&) = 0;

  // Requests a page-scale animation based on the specified rect.
  virtual void ZoomToFindInPageRect(const WebRect&) = 0;

  // Sets the display mode of the web app.
  virtual void SetDisplayMode(blink::mojom::DisplayMode) = 0;

  // Sets the ratio as computed by computePageScaleConstraints.
  // TODO(oshima): Remove this once the device scale factor implementation is
  // fully migrated to use zooming mechanism.
  virtual void SetDeviceScaleFactor(float) = 0;

  // Sets the additional zoom factor used for device scale factor. This is used
  // to scale the content by the device scale factor, without affecting zoom
  // level.
  virtual void SetZoomFactorForDeviceScaleFactor(float) = 0;

  virtual float ZoomFactorForDeviceScaleFactor() = 0;

  // Resize the view at the same time as changing the state of the top
  // controls. If |browser_controls_shrink_layout| is true, the embedder shrunk
  // the WebView size by the browser controls height.
  virtual void ResizeWithBrowserControls(
      const WebSize&,
      float top_controls_height,
      float bottom_controls_height,
      bool browser_controls_shrink_layout) = 0;

  // Same as ResizeWithBrowserControls, but keeps the same BrowserControl
  // settings.
  virtual void Resize(const WebSize&) = 0;

  virtual WebSize GetSize() = 0;

  // Auto-Resize -----------------------------------------------------------

  // In auto-resize mode, the view is automatically adjusted to fit the html
  // content within the given bounds.
  virtual void EnableAutoResizeMode(const WebSize& min_size,
                                    const WebSize& max_size) = 0;

  // Turn off auto-resize.
  virtual void DisableAutoResizeMode() = 0;

  // Media ---------------------------------------------------------------

  // Performs the specified plugin action on the node at the given location.
  virtual void PerformPluginAction(const PluginAction&,
                                   const gfx::Point& location) = 0;

  // Notifies WebView when audio is started or stopped.
  virtual void AudioStateChanged(bool is_audio_playing) = 0;

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
  virtual void EnableDeviceEmulation(const WebDeviceEmulationParams&) = 0;

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

  // Overrides the page's background and base background color. You
  // can use this to enforce a transparent background, which is useful if you
  // want to have some custom background rendered behind the widget.
  //
  // These may are only called for composited WebViews.
  virtual void SetBackgroundColorOverride(SkColor) {}
  virtual void ClearBackgroundColorOverride() {}
  virtual void SetBaseBackgroundColorOverride(SkColor) {}
  virtual void ClearBaseBackgroundColorOverride() {}

  // Scheduling -----------------------------------------------------------

  virtual PageScheduler* Scheduler() const = 0;

  // Visibility -----------------------------------------------------------

  // Sets the visibility of the WebView.
  virtual void SetVisibilityState(PageVisibilityState visibility_state,
                                  bool is_initial_state) = 0;
  virtual PageVisibilityState GetVisibilityState() = 0;

  // FrameOverlay ----------------------------------------------------------

  // Overlay this WebView with a solid color.
  virtual void SetMainFrameOverlayColor(SkColor) = 0;

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

  // Dispatches a pagehide event, freezes a page and hooks page eviction.
  virtual void PutPageIntoBackForwardCache() = 0;

  // Unhooks eviction, resumes a page and dispatches a pageshow event.
  virtual void RestorePageFromBackForwardCache(
      base::TimeTicks navigation_start) = 0;

  // Testing functionality for TestRunner ---------------------------------

  // Force the webgl context to fail so that webglcontextcreationerror
  // event gets generated/tested.
  virtual void ForceNextWebGLContextCreationToFail() = 0;

  // Force the drawing buffer used by webgl contexts to fail so that the webgl
  // context's ability to deal with that failure gracefully can be tested.
  virtual void ForceNextDrawingBufferCreationToFail() = 0;

  // Autoplay configuration -----------------------------------------------

  // Sets the autoplay flags for this webview's page.
  // The valid flags are defined in
  // third_party/blink/public/platform/autoplay.mojom
  virtual void AddAutoplayFlags(int32_t flags) = 0;
  virtual void ClearAutoplayFlags() = 0;
  virtual int32_t AutoplayFlagsForTest() = 0;

  // Non-composited support -----------------------------------------------

  // Called to paint the rectangular region within the WebView's main frame
  // onto the specified canvas at (viewport.x, viewport.y). This is to provide
  // support for non-composited WebViews, and is used to paint into a
  // PaintCanvas being supplied by another (composited) WebView.
  //
  // Before calling PaintContent(), the caller must ensure the lifecycle of the
  // widget's frame is clean by calling UpdateLifecycle(LifecycleUpdate::All).
  // It is okay to call paint multiple times once the lifecycle is clean,
  // assuming no other changes are made to the WebWidget (e.g., once
  // events are processed, it should be assumed that another call to
  // UpdateLifecycle is warranted before painting again). Paints starting from
  // the main LayoutView's property tree state, thus ignoring any transient
  // transormations (e.g. pinch-zoom, dev tools emulation, etc.).
  //
  // The painting will be performed without applying the DevicePixelRatio as
  // scaling is expected to already be applied to the PaintCanvas by the
  // composited WebView which supplied the PaintCanvas. The canvas state may
  // be modified and should be saved before calling this method and restored
  // after.
  virtual void PaintContent(cc::PaintCanvas*, const gfx::Rect& viewport) = 0;

  // Suspend and resume ---------------------------------------------------

  // TODO(lfg): Remove this once the refactor of WebView/WebWidget is
  // completed.
  virtual WebWidget* MainFrameWidget() = 0;

  // Portals --------------------------------------------------------------

  // Informs the page that it is inside a portal.
  virtual void SetInsidePortal(bool inside_portal) = 0;

  // Use to transfer TextAutosizer state from the local main frame renderer to
  // remote main frame renderers.
  virtual void SetTextAutosizePageInfo(const WebTextAutosizerPageInfo&) {}

 protected:
  ~WebView() = default;
};

}  // namespace blink

#endif

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
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom-shared.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {

class PageScheduler;
class WebFrame;
class WebHitTestResult;
class WebLocalFrame;
class WebPageImportanceSignals;
class WebPrerendererClient;
class WebRemoteFrame;
class WebSettings;
class WebString;
class WebViewClient;
class WebWidgetClient;
struct WebDeviceEmulationParams;
struct WebFloatPoint;
struct WebPluginAction;
struct WebPoint;
struct WebWindowFeatures;

class WebView : protected WebWidget {
 public:
  BLINK_EXPORT static const double kTextSizeMultiplierRatio;
  BLINK_EXPORT static const double kMinTextSizeMultiplier;
  BLINK_EXPORT static const double kMaxTextSizeMultiplier;

  enum StyleInjectionTarget {
    kInjectStyleInAllFrames,
    kInjectStyleInTopFrameOnly
  };

  // WebWidget overrides.
  using WebWidget::Close;
  using WebWidget::LifecycleUpdate;
  using WebWidget::Size;
  using WebWidget::Resize;
  using WebWidget::ResizeVisualViewport;
  using WebWidget::DidEnterFullscreen;
  using WebWidget::DidExitFullscreen;
  using WebWidget::BeginFrame;
  using WebWidget::UpdateAllLifecyclePhases;
  using WebWidget::UpdateLifecycle;
  using WebWidget::PaintContent;
  using WebWidget::LayoutAndPaintAsync;
  using WebWidget::CompositeAndReadbackAsync;
  using WebWidget::ThemeChanged;
  using WebWidget::HandleInputEvent;
  using WebWidget::DispatchBufferedTouchEvents;
  using WebWidget::SetCursorVisibilityState;
  using WebWidget::ApplyViewportChanges;
  using WebWidget::MouseCaptureLost;
  using WebWidget::SetFocus;
  using WebWidget::SelectionBounds;
  using WebWidget::IsAcceleratedCompositingActive;
  using WebWidget::IsWebView;
  using WebWidget::IsPagePopup;
  using WebWidget::WillCloseLayerTreeView;
  using WebWidget::DidAcquirePointerLock;
  using WebWidget::DidNotAcquirePointerLock;
  using WebWidget::DidLosePointerLock;
  using WebWidget::BackgroundColor;
  using WebWidget::GetPagePopup;

  // Initialization ------------------------------------------------------

  // Creates a WebView that is NOT yet initialized. To complete initialization,
  // call WebLocalFrame::CreateMainFrame() or WebRemoteFrame::CreateMainFrame()
  // as appropriate. It is legal to modify settings before completing
  // initialization.
  //
  // client may be null, while PageVisibilityState defines the initial
  // visibility of the page.
  BLINK_EXPORT static WebView* Create(WebViewClient*,
                                      WebWidgetClient*,
                                      mojom::PageVisibilityState,
                                      WebView* opener);

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

  // Smooth scroll the root layer to |targetX|, |targetY| in |durationMs|.
  virtual void SmoothScroll(int target_x, int target_y, long duration_ms) {}

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

  // Updates the zoom limits for this view.
  virtual void ZoomLimitsChanged(double minimum_zoom_level,
                                 double maximum_zoom_level) = 0;

  // Helper functions to convert between zoom level and zoom factor.  zoom
  // factor is zoom percent / 100, so 300% = 3.0.
  BLINK_EXPORT static double ZoomLevelToZoomFactor(double zoom_level);
  BLINK_EXPORT static double ZoomFactorToZoomLevel(double factor);

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
  // partial CSS pixels. The offset will be clamped so the visual viewport
  // stays within the frame's bounds.
  virtual void SetVisualViewportOffset(const WebFloatPoint&) = 0;

  // Gets the visual viewport's current offset within the page's main frame,
  // in partial CSS pixels.
  virtual WebFloatPoint VisualViewportOffset() const = 0;

  // Get the visual viewport's size in CSS pixels.
  virtual WebFloatSize VisualViewportSize() const = 0;

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
  virtual WebSize ContentsPreferredMinimumSize() = 0;

  // Sets the display mode of the web app.
  virtual void SetDisplayMode(WebDisplayMode) = 0;

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
  // controls. If |browserControlsShrinkLayout| is true, the embedder shrunk the
  // WebView size by the browser controls height.
  virtual void ResizeWithBrowserControls(
      const WebSize&,
      float top_controls_height,
      float bottom_controls_height,
      bool browser_controls_shrink_layout) = 0;

  // Auto-Resize -----------------------------------------------------------

  // In auto-resize mode, the view is automatically adjusted to fit the html
  // content within the given bounds.
  virtual void EnableAutoResizeMode(const WebSize& min_size,
                                    const WebSize& max_size) = 0;

  // Turn off auto-resize.
  virtual void DisableAutoResizeMode() = 0;

  // Media ---------------------------------------------------------------

  // Performs the specified plugin action on the node at the given location.
  virtual void PerformPluginAction(const WebPluginAction&,
                                   const WebPoint& location) = 0;

  // Notifies WebView when audio is started or stopped.
  virtual void AudioStateChanged(bool is_audio_playing) = 0;

  // Data exchange -------------------------------------------------------

  // Do a hit test at given point and return the HitTestResult.
  WebHitTestResult HitTestResultAt(const WebPoint&) override = 0;

  // Do a hit test equivalent to what would be done for a GestureTap event
  // that has width/height corresponding to the supplied |tapArea|.
  virtual WebHitTestResult HitTestResultForTap(const WebPoint& tap_point,
                                               const WebSize& tap_area) = 0;

  // Support for resource loading initiated by plugins -------------------

  // Returns next unused request identifier which is unique within the
  // parent Page.
  virtual unsigned long CreateUniqueIdentifierForRequest() = 0;

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

  // Hides any popup (suggestions, selects...) that might be showing.
  virtual void HidePopups() = 0;

  // Visited link state --------------------------------------------------

  // Tells all WebView instances to update the visited link state for the
  // specified hash.
  BLINK_EXPORT static void UpdateVisitedLinkState(unsigned long long hash);

  // Tells all WebView instances to update the visited state for all
  // their links. Use invalidateVisitedLinkHashes to inform that the visitedlink
  // table was changed and the salt was changed too. And all cached visitedlink
  // hashes need to be recalculated.
  BLINK_EXPORT static void ResetVisitedLinkState(
      bool invalidate_visited_link_hashes);

  // Custom colors -------------------------------------------------------

  virtual void SetSelectionColors(unsigned active_background_color,
                                  unsigned active_foreground_color,
                                  unsigned inactive_background_color,
                                  unsigned inactive_foreground_color) = 0;

  // Modal dialog support ------------------------------------------------

  // Call these methods before and after running a nested, modal event loop
  // to suspend script callbacks and resource loads.
  BLINK_EXPORT static void WillEnterModalLoop();
  BLINK_EXPORT static void DidExitModalLoop();

  virtual void SetShowPaintRects(bool) = 0;
  virtual void SetShowFPSCounter(bool) = 0;
  virtual void SetShowScrollBottleneckRects(bool) = 0;

  // Scheduling -----------------------------------------------------------

  virtual PageScheduler* Scheduler() const = 0;

  // Visibility -----------------------------------------------------------

  // Sets the visibility of the WebView.
  virtual void SetVisibilityState(mojom::PageVisibilityState visibility_state,
                                  bool is_initial_state) {}

  // PageOverlay ----------------------------------------------------------

  // Overlay this WebView with a solid color.
  virtual void SetPageOverlayColor(SkColor) = 0;

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

  // Suspend and resume ---------------------------------------------------

  // Pausing and unpausing current scheduled tasks.
  virtual void PausePageScheduledTasks(bool paused) = 0;

  // TODO(lfg): Remove this once the refactor of WebView/WebWidget is
  // completed.
  WebWidget* GetWidget() { return this; }

 protected:
  ~WebView() = default;
};

}  // namespace blink

#endif

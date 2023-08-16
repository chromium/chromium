// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "content/browser/renderer_host/display_feature.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/page_visibility_state.h"
#include "content/public/common/widget_type.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/fullscreen.mojom.h"
#include "third_party/blink/public/mojom/frame/intrinsic_sizing_info.mojom-forward.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "third_party/blink/public/mojom/widget/record_content_to_visible_time_request.mojom.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/base/ime/mojom/text_input_state.mojom-forward.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/display.h"
#include "ui/display/screen_infos.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace ui {
class Compositor;
class Cursor;
class LatencyInfo;
class TouchEvent;
enum class DomCode;
struct DidOverscrollParams;
}  // namespace ui

namespace content {

class CursorManager;
class MouseWheelPhaseHandler;
class RenderWidgetHostImpl;
class RenderWidgetHostViewBaseObserver;
class SyntheticGestureTarget;
class TextInputManager;
class TouchSelectionControllerClientManager;
class WebContentsAccessibility;
class DelegatedFrameHost;

// Basic implementation shared by concrete RenderWidgetHostView subclasses.
class CONTENT_EXPORT RenderWidgetHostViewBase : public RenderWidgetHostView {
 public:
  // The TooltipObserver is used in browser tests only.
  class CONTENT_EXPORT TooltipObserver {
   public:
    virtual ~TooltipObserver() = default;

    virtual void OnTooltipTextUpdated(const std::u16string& tooltip_text) = 0;
  };

  RenderWidgetHostViewBase(const RenderWidgetHostViewBase&) = delete;
  RenderWidgetHostViewBase& operator=(const RenderWidgetHostViewBase&) = delete;

  // Returns the focused RenderWidgetHost inside this |view|'s RWH.
  RenderWidgetHostImpl* GetFocusedWidget() const;

  // RenderWidgetHostView implementation.
  RenderWidgetHost* GetRenderWidgetHost() final;
  ui::TextInputClient* GetTextInputClient() override;
  void Show() final;
  void WasUnOccluded() override {}
  void WasOccluded() override {}
  std::u16string GetSelectedText() override;
  bool IsMouseLocked() override;
  bool GetIsMouseLockedUnadjustedMovementForTesting() override;
  bool CanBeMouseLocked() override;
  bool AccessibilityHasFocus() override;
  bool LockKeyboard(absl::optional<base::flat_set<ui::DomCode>> codes) override;
  void SetBackgroundColor(SkColor color) override;
  absl::optional<SkColor> GetBackgroundColor() override;
  void CopyBackgroundColorIfPresentFrom(
      const RenderWidgetHostView& other) override;
  void UnlockKeyboard() override;
  bool IsKeyboardLocked() override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  gfx::Size GetVisibleViewportSize() override;
  void SetInsets(const gfx::Insets& insets) override;
  bool IsSurfaceAvailableForCopy() override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> CreateVideoCapturer()
      override;
  display::ScreenInfo GetScreenInfo() const override;
  display::ScreenInfos GetScreenInfos() const override;

  // Identical to `CopyFromSurface()`, except that this method issues the
  // `viz::CopyOutputRequest` against the exact `viz::Surface` currently
  // embedded by this View, while `CopyFromSurface()` may return a copy of any
  // Surface associated with this View, generated after the current Surface. The
  // caller is responsible for making sure that the target Surface is embedded
  // and available for copy when this API is called. This Surface can be removed
  // from the UI after this call.
  //
  // TODO(https://crbug.com/1467314): merge this API into `CopyFromSurface()`,
  // and enable it fully on Android.
  virtual void CopyFromExactSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback);

  // For HiDPI capture mode, allow applying a render scale multiplier
  // which modifies the effective device scale factor. Use a scale
  // of 1.0f (exactly) to disable the feature after it was used.
  void SetScaleOverrideForCapture(float scale);
  float GetScaleOverrideForCapture() const;

  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  float GetDeviceScaleFactor() const final;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;
  ui::mojom::VirtualKeyboardMode GetVirtualKeyboardMode() override;
  void NotifyVirtualKeyboardOverlayRect(
      const gfx::Rect& keyboard_rect) override {}
  bool IsHTMLFormPopup() const override;

  // This only needs to be overridden by RenderWidgetHostViewBase subclasses
  // that handle content embedded within other RenderWidgetHostViews.
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;

  virtual void UpdateIntrinsicSizingInfo(
      blink::mojom::IntrinsicSizingInfoPtr sizing_info);

  static void CopyMainAndPopupFromSurface(
      base::WeakPtr<RenderWidgetHostImpl> main_host,
      base::WeakPtr<DelegatedFrameHost> main_frame_host,
      base::WeakPtr<RenderWidgetHostImpl> popup_host,
      base::WeakPtr<DelegatedFrameHost> popup_frame_host,
      const gfx::Rect& src_subrect,
      const gfx::Size& dst_size,
      float scale_factor,
      base::OnceCallback<void(const SkBitmap&)> callback);

  void SetWidgetType(WidgetType widget_type);

  WidgetType GetWidgetType();

  virtual void SendInitialPropertiesIfNeeded() {}

  // Called when screen information or native widget bounds change.
  virtual void UpdateScreenInfo();

  // Called by the TextInputManager to notify the view about being removed from
  // the list of registered views, i.e., TextInputManager is no longer tracking
  // TextInputState from this view. The RWHV should reset |text_input_manager_|
  // to nullptr.
  void DidUnregisterFromTextInputManager(TextInputManager* text_input_manager);

  // Informs the view that the renderer's visual properties have been updated
  // and a new viz::LocalSurfaceId has been allocated.
  virtual viz::ScopedSurfaceIdAllocator DidUpdateVisualProperties(
      const cc::RenderFrameMetadata& metadata);

  base::WeakPtr<RenderWidgetHostViewBase> GetWeakPtr();

  //----------------------------------------------------------------------------
  // The following methods can be overridden by derived classes.

  // Returns the root-view associated with this view. Always returns |this| for
  // non-embeddable derived views.
  virtual RenderWidgetHostViewBase* GetRootView();

  // Notifies the View that the renderer text selection has changed.
  virtual void SelectionChanged(const std::u16string& text,
                                size_t offset,
                                const gfx::Range& range);

  // The requested size of the renderer. May differ from GetViewBounds().size()
  // when the view requires additional throttling.
  virtual gfx::Size GetRequestedRendererSize();

  // Returns the current capture sequence number.
  virtual uint32_t GetCaptureSequenceNumber() const;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetCompositorViewportPixelSize();

  // If mouse wheels can only specify the number of ticks of some static
  // multiplier constant, this method returns that constant (in DIPs). If mouse
  // wheels can specify an arbitrary delta this returns 0.
  virtual int GetMouseWheelMinimumGranularity() const;

  // Called prior to forwarding input event messages to the renderer, giving
  // the view a chance to perform in-process event filtering or processing.
  // Return values of |NOT_CONSUMED| or |UNKNOWN| will result in |input_event|
  // being forwarded.
  virtual blink::mojom::InputEventResultState FilterInputEvent(
      const blink::WebInputEvent& input_event);

  virtual void WheelEventAck(const blink::WebMouseWheelEvent& event,
                             blink::mojom::InputEventResultState ack_result);

  virtual void GestureEventAck(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result,
      blink::mojom::ScrollResultDataPtr scroll_result_data);

  virtual void ChildDidAckGestureEvent(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result,
      blink::mojom::ScrollResultDataPtr scroll_result_data);

  // Create a platform specific SyntheticGestureTarget implementation that will
  // be used to inject synthetic input events.
  virtual std::unique_ptr<SyntheticGestureTarget>
  CreateSyntheticGestureTarget() = 0;

  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget();
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible();
  virtual gfx::NativeViewAccessible
  AccessibilityGetNativeViewAccessibleForWindow();
  virtual void SetMainFrameAXTreeID(ui::AXTreeID id) {}
  // Informs that the focused DOM node has changed.
  virtual void FocusedNodeChanged(bool is_editable_node,
                                  const gfx::Rect& node_bounds_in_screen) {}

  // Requests to start stylus writing and returns true if successful.
  virtual bool RequestStartStylusWriting();

  // Notify whether the hovered element action is stylus writable or not.
  virtual void NotifyHoverActionStylusWritable(bool stylus_writable) {}

  // This message is received when the stylus writable element is focused.
  // It receives the focused edit element bounds and the current caret bounds
  // needed for stylus writing service. These bounds would be empty when the
  // stylus writable element could not be focused.
  virtual void OnEditElementFocusedForStylusWriting(
      const gfx::Rect& focused_edit_bounds,
      const gfx::Rect& caret_bounds) {}

  // Invalidates the `viz::SurfaceAllocationGroup` of this View. Also
  // invalidates `viz::SurfaceId` of it. This should be used when no previous
  // frame drawn by this view is preferred as a fallback.
  virtual void InvalidateLocalSurfaceIdAndAllocationGroup() = 0;

  // This method will clear any cached fallback surface. For use in response to
  // a CommitPending where there is no content for TakeFallbackContentFrom.
  virtual void ClearFallbackSurfaceForCommitPending() {}
  // This method will reset the fallback to the first surface after navigation.
  virtual void ResetFallbackToFirstNavigationSurface() = 0;

  // Requests a new CompositorFrame from the renderer. This is done by
  // allocating a new viz::LocalSurfaceId which forces a commit and draw.
  virtual bool RequestRepaintForTesting();

  // Because the associated remote WebKit instance can asynchronously
  // prevent-default on a dispatched touch event, the touch events are queued in
  // the GestureRecognizer until invocation of ProcessAckedTouchEvent releases
  // it to be consumed (when |ack_result| is NOT_CONSUMED OR NO_CONSUMER_EXISTS)
  // or ignored (when |ack_result| is CONSUMED).
  // |touch|'s coordinates are in the coordinate space of the view to which it
  // was targeted.
  virtual void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result);

  virtual void DidOverscroll(const ui::DidOverscrollParams& params) {}

  virtual void DidStopFlinging() {}

  // Returns the ID associated with the CompositorFrameSink of this view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;

  // Returns the LocalSurfaceId allocated by the parent client for this view.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() const = 0;

  // Called whenever the browser receives updated hit test data from viz.
  virtual void NotifyHitTestRegionUpdated(
      const viz::AggregatedHitTestRegion& region) {}

  // Indicates whether the widget has resized or moved within its embedding
  // page during a feature-parameter-determined time interval.
  virtual bool ScreenRectIsUnstableFor(const blink::WebInputEvent& event);

  // See kTargetFrameMovedRecentlyForIOv2 in web_input_event.h.
  virtual bool ScreenRectIsUnstableForIOv2For(
      const blink::WebInputEvent& event);

  virtual void PreProcessMouseEvent(const blink::WebMouseEvent& event) {}
  virtual void PreProcessTouchEvent(const blink::WebTouchEvent& event) {}

  void ProcessMouseEvent(const blink::WebMouseEvent& event,
                         const ui::LatencyInfo& latency);
  void ProcessMouseWheelEvent(const blink::WebMouseWheelEvent& event,
                              const ui::LatencyInfo& latency);
  void ProcessTouchEvent(const blink::WebTouchEvent& event,
                         const ui::LatencyInfo& latency);
  virtual void ProcessGestureEvent(const blink::WebGestureEvent& event,
                                   const ui::LatencyInfo& latency);

  // Transform a point that is in the coordinate space of a Surface that is
  // embedded within the RenderWidgetHostViewBase's Surface to the
  // coordinate space of an embedding, or embedded, Surface. Typically this
  // means that a point was received from an out-of-process iframe's
  // RenderWidget and needs to be translated to viewport coordinates for the
  // root RWHV, in which case this method is called on the root RWHV with the
  // out-of-process iframe's SurfaceId.
  // Returns false when this attempts to transform a point between coordinate
  // spaces of surfaces where one does not contain the other. To transform
  // between sibling surfaces, the point must be transformed to the root's
  // coordinate space as an intermediate step.
  bool TransformPointToLocalCoordSpace(const gfx::PointF& point,
                                       const viz::SurfaceId& original_surface,
                                       gfx::PointF* transformed_point);

  // Given a RenderWidgetHostViewBase that renders to a Surface that is
  // contained within this class' Surface, find the relative transform between
  // the Surfaces and apply it to a point. Returns false if a Surface has not
  // yet been created or if |target_view| is not a descendant RWHV from our
  // client.
  virtual bool TransformPointToCoordSpaceForView(
      const gfx::PointF& point,
      RenderWidgetHostViewBase* target_view,
      gfx::PointF* transformed_point);

  // On success, returns true and modifies |*transform| to represent the
  // transformation mapping a point in the coordinate space of this view
  // into the coordinate space of the target view.
  // On Failure, returns false, and leaves |*transform| unchanged.
  // This function will fail if viz hit testing is not enabled, or if either
  // this view or the target view has no current FrameSinkId. The latter may
  // happen if either view is not currently visible in the viewport.
  // This function is useful if there are multiple points to transform between
  // the same two views. |target_view| must be non-null.
  bool GetTransformToViewCoordSpace(RenderWidgetHostViewBase* target_view,
                                    gfx::Transform* transform);

  // Subclass identifier for RenderWidgetHostViewChildFrames. This is useful
  // to be able to know if this RWHV is embedded within another RWHV. If
  // other kinds of embeddable RWHVs are created, this should be renamed to
  // a more generic term -- in which case, static casts to RWHVChildFrame will
  // need to also be resolved.
  virtual bool IsRenderWidgetHostViewChildFrame();

  // Obtains the root window FrameSinkId.
  virtual viz::FrameSinkId GetRootFrameSinkId();

  // Returns the SurfaceId currently in use by the renderer to submit compositor
  // frames.
  virtual viz::SurfaceId GetCurrentSurfaceId() const = 0;

  // Returns true if this view's size have been initialized.
  virtual bool HasSize() const;

  // Returns true if the visual properties should be sent to the renderer at
  // this time. This function is intended for subclasses to suppress
  // synchronization, the default implementation returns true.
  virtual bool CanSynchronizeVisualProperties();

  // Extracts information about any active pointers and cancels any existing
  // active pointers by dispatching synthetic cancel events.
  virtual std::vector<std::unique_ptr<ui::TouchEvent>>
  ExtractAndCancelActiveTouches();

  // Used to transfer pointer state from one view to another. It recreates the
  // pointer state by dispatching touch down events.
  virtual void TransferTouches(
      const std::vector<std::unique_ptr<ui::TouchEvent>>& touches) {}

  virtual void SetLastPointerType(ui::EventPointerType last_pointer_type) {}

  //----------------------------------------------------------------------------
  // The following methods are related to IME.
  // TODO(ekaramad): Most of the IME methods should not stay virtual after IME
  // is implemented for OOPIF. After fixing IME, mark the corresponding methods
  // non-virtual (https://crbug.com/578168).

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(
      const ui::mojom::TextInputState& text_input_state);

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition();

  // Notifies the view that the renderer selection bounds has changed.
  // Selection bounds are described as a focus bound which is the current
  // position of caret on the screen, as well as the anchor bound which is the
  // starting position of the selection. `bounding_box` is the bounds of the
  // rectangle enclosing the selection region. The coordinates are with respect
  // to RenderWidget's window's origin. Focus and anchor bound are represented
  // as gfx::Rect.
  virtual void SelectionBoundsChanged(const gfx::Rect& anchor_rect,
                                      base::i18n::TextDirection anchor_dir,
                                      const gfx::Rect& focus_rect,
                                      base::i18n::TextDirection focus_dir,
                                      const gfx::Rect& bounding_box,
                                      bool is_anchor_first);

  // Updates the range of the marked text in an IME composition, the visible
  // line bounds, or both.
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const absl::optional<std::vector<gfx::Rect>>& character_bounds,
      const absl::optional<std::vector<gfx::Rect>>& line_bounds);

  //----------------------------------------------------------------------------
  // The following pure virtual methods are implemented by derived classes.

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos| using
  // |anchor_rect|. See OwnedWindowAnchor in //ui/base/ui_base_types.h for more
  // details.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& bounds,
                           const gfx::Rect& anchor_rect) = 0;

  // Sets the cursor for this view to the one specified.
  virtual void UpdateCursor(const ui::Cursor& cursor) = 0;

  // Changes the cursor that is displayed on screen. This may or may not match
  // the current cursor's view which was set by UpdateCursor.
  virtual void DisplayCursor(const ui::Cursor& cursor);

  // Views that manage cursors for window return a CursorManager. Other views
  // return nullptr.
  virtual CursorManager* GetCursorManager();

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderProcessGone() = 0;

  // `page_visibility` is kHiddenButPainting when the view is shown even though
  // the web contents should be hidden, e.g., because a background tab is
  // screen captured.
  virtual void ShowWithVisibility(PageVisibilityState page_visibility) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy();

  // Calls UpdateTooltip if the view is under the cursor.
  virtual void UpdateTooltipUnderCursor(const std::u16string& tooltip_text) {}

  // Updates the tooltip text and displays the requested tooltip on the screen.
  // An empty string will clear a visible tooltip.
  virtual void UpdateTooltip(const std::u16string& tooltip_text) {}

  // Updates the tooltip text and its position and displays the requested
  // tooltip on the screen. The |bounds| parameter corresponds to the bounds of
  // the renderer-side element (in widget-relative DIPS) on which the tooltip
  // should appear to be anchored.
  virtual void UpdateTooltipFromKeyboard(const std::u16string& tooltip_text,
                                         const gfx::Rect& bounds) {}

  // Hides tooltips that are still visible and were triggered from a keypress.
  // Doesn't impact tooltips that were triggered from the cursor.
  virtual void ClearKeyboardTriggeredTooltip() {}

  // Transforms |point| to be in the coordinate space of browser compositor's
  // surface. This is in DIP.
  virtual void TransformPointToRootSurface(gfx::PointF* point);

  // Gets the bounds of the top-level window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  // Dispatched when the main frame associated with a `RenderWidgetHostView` is
  // being navigated to a different Document (won't be dispatched for
  // same-document navigations).
  //
  // "PreCommit" means the new page hasn't been marked as visible yet.
  virtual void DidNavigateMainFramePreCommit();

  // Gives a chance to the caller to perform some task AFTER the page is
  // unloaded and stored in the BFCache.
  virtual void DidEnterBackForwardCache() {}

  // Called by WebContentsImpl to notify the view about a change in visibility
  // of context menu. The view can then perform platform specific tasks and
  // changes.
  virtual void SetShowingContextMenu(bool showing) {}

  virtual void OnAutoscrollStart();

  // Gets the DisplayFeature whose offset and mask_length are expressed in DIPs
  // relative to the view. See display_feature.h for more details.
  virtual absl::optional<DisplayFeature> GetDisplayFeature() = 0;

  virtual void SetDisplayFeatureForTesting(
      const DisplayFeature* display_feature) = 0;

  // Returns the associated RenderWidgetHostImpl.
  RenderWidgetHostImpl* host() const { return host_; }

  // Process swap messages sent before |frame_token| in RenderWidgetHostImpl.
  void OnFrameTokenChangedForView(uint32_t frame_token,
                                  base::TimeTicks activation_time);

  // Add and remove observers for lifetime event notifications. The order in
  // which notifications are sent to observers is undefined. Clients must be
  // sure to remove the observer before they go away.
  void AddObserver(RenderWidgetHostViewBaseObserver* observer);
  void RemoveObserver(RenderWidgetHostViewBaseObserver* observer);

  // Returns a reference to the current instance of TextInputManager. The
  // reference is obtained from RenderWidgetHostDelegate. The first time a non-
  // null reference is obtained, its value is cached in |text_input_manager_|
  // and this view is registered with it. The RWHV will unregister from the
  // TextInputManager if it is destroyed or if the TextInputManager itself is
  // destroyed. The unregistration of the RWHV from TextInputManager is
  // necessary and must be done by explicitly calling
  // TextInputManager::Unregister.
  // It is safer to use this method rather than directly dereferencing
  // |text_input_manager_|.
  TextInputManager* GetTextInputManager();

  void StopFling();

  void set_is_currently_scrolling_viewport(
      bool is_currently_scrolling_viewport) {
    is_currently_scrolling_viewport_ = is_currently_scrolling_viewport;
  }

  bool is_currently_scrolling_viewport() {
    return is_currently_scrolling_viewport_;
  }

  virtual void DidNavigate();

  // Called when the RenderWidgetHostImpl establishes a connection to the
  // renderer process Widget.
  virtual void OnRendererWidgetCreated() {}

  virtual WebContentsAccessibility* GetWebContentsAccessibility();

  void set_is_evicted() { is_evicted_ = true; }
  void reset_is_evicted() { is_evicted_ = false; }
  bool is_evicted() { return is_evicted_; }

  // SetContentBackgroundColor is called when the renderer wants to update the
  // view's background color.
  void SetContentBackgroundColor(SkColor color);
  absl::optional<SkColor> content_background_color() const {
    return content_background_color_;
  }

  void SetTooltipObserverForTesting(TooltipObserver* observer);

  virtual ui::Compositor* GetCompositor();

  virtual void EnterFullscreenMode(
      const blink::mojom::FullscreenOptions& options) {}
  virtual void ExitFullscreenMode() {}
  virtual void LockOrientation(
      device::mojom::ScreenOrientationLockType orientation) {}
  virtual void UnlockOrientation() {}
  virtual void SetHasPersistentVideo(bool has_persistent_video) {}

  bool HasFallbackSurfaceForTesting() const { return HasFallbackSurface(); }

 protected:
  explicit RenderWidgetHostViewBase(RenderWidgetHost* host);
  ~RenderWidgetHostViewBase() override;

  void NotifyObserversAboutShutdown();

  virtual MouseWheelPhaseHandler* GetMouseWheelPhaseHandler();

  // Applies background color without notifying the RenderWidget about
  // opaqueness changes. This allows us to, when navigating to a new page,
  // transfer this color to that page. This allows us to pass this background
  // color to new views on navigation.
  virtual void UpdateBackgroundColor() = 0;

  // Stops flinging if a GSU event with momentum phase is sent to the renderer
  // but not consumed.
  virtual void StopFlingingIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result);

  // If |event| is a touchpad pinch or double tap event for which we've sent a
  // synthetic wheel event, forward the |event| to the renderer, subject to
  // |ack_result| which is the ACK result of the synthetic wheel.
  virtual void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result);

  virtual bool HasFallbackSurface() const;

  // Checks the combination of RenderWidgetHostImpl hidden state and
  // `page_visibility` and calls NotifyHostAndDelegateOnWasShown,
  // RequestSuccessfulPresentationTimeFromHostOrDelegate or
  // CancelSuccessfulPresentationTimeRequestForHostAndDelegate as appropriate.
  //
  // This starts and stops tab switch latency measurements as needed so most
  // platforms should call this from ShowWithVisibility. Android does not
  // implement tab switch latency measurements so it calls
  // RenderWidgetHostImpl::WasShown and DelegatedFrameHost::WasShown directly
  // instead.
  void OnShowWithPageVisibility(PageVisibilityState page_visibility);

  void UpdateSystemCursorSize(const gfx::Size& cursor_size);

  // Updates the active state by replicating it to the renderer.
  void UpdateActiveState(bool active);

  // Each platform should override this to call RenderWidgetHostImpl::WasShown
  // and DelegatedFrameHost::WasShown, and do any platform-specific bookkeeping
  // needed.  The given `visible_time_request`, if any, should be passed to
  // DelegatedFrameHost::WasShown if there is a saved frame or
  // RenderWidgetHostImpl if not.
  virtual void NotifyHostAndDelegateOnWasShown(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          visible_time_request) = 0;

  // Each platform should override this to pass `visible_time_request`, which
  // will never be null, to
  // DelegatedFrameHost::RequestSuccessfulPresentationTimeForNextFrame if there
  // is a saved frame or
  // RenderWidgetHostImpl::RequestSuccessfulPresentationTimeForNextFrame if not,
  // after doing and platform-specific bookkeeping needed.
  virtual void RequestSuccessfulPresentationTimeFromHostOrDelegate(
      blink::mojom::RecordContentToVisibleTimeRequestPtr
          visible_time_request) = 0;

  // Each platform should override this to call
  // DelegatedFrameHost::CancelSuccessfulPresentationTimeRequest and
  // RenderWidgetHostImpl::CancelSuccessfulPresentationTimeRequest, after doing
  // and platform-specific bookkeeping needed.
  virtual void CancelSuccessfulPresentationTimeRequestForHostAndDelegate() = 0;

  // The model object. Access is protected to allow access to
  // RenderWidgetHostViewChildFrame.
  raw_ptr<RenderWidgetHostImpl, DanglingUntriaged> host_;

  // Whether this view is a frame or a popup.
  WidgetType widget_type_ = WidgetType::kFrame;

  // Cached information about the renderer's display environment.
  display::ScreenInfos screen_infos_;

  float scale_override_for_capture_ = 1.0f;

  // Indicates whether keyboard lock is active for this view.
  bool keyboard_locked_ = false;

  // Indicates whether the scroll offset of the root layer is at top, i.e.,
  // whether scroll_offset.y() == 0.
  bool is_scroll_offset_at_top_ = true;

  // A reference to current TextInputManager instance this RWHV is registered
  // with. This is initially nullptr until the first time the view calls
  // GetTextInputManager(). It also becomes nullptr when TextInputManager is
  // destroyed before the RWHV is destroyed.
  raw_ptr<TextInputManager> text_input_manager_ = nullptr;

  // The background color used in the current renderer.
  absl::optional<SkColor> content_background_color_;

  // The default background color used before getting the
  // |content_background_color|.
  absl::optional<SkColor> default_background_color_;

  bool is_currently_scrolling_viewport_ = false;

  raw_ptr<TooltipObserver> tooltip_observer_for_testing_ = nullptr;

  // Cursor size in logical pixels, obtained from the OS. This value is general
  // to all displays.
  gfx::Size system_cursor_size_;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      BrowserSideFlingBrowserTest,
      EarlyTouchscreenFlingCancelationOnInertialGSUAckNotConsumed);
  FRIEND_TEST_ALL_PREFIXES(
      BrowserSideFlingBrowserTest,
      EarlyTouchpadFlingCancelationOnInertialGSUAckNotConsumed);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostDelegatedInkMetadataTest,
                           FlagGetsSetFromRenderFrameMetadata);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostInputEventRouterTest,
                           QueryResultAfterChildViewDead);
  FRIEND_TEST_ALL_PREFIXES(DelegatedInkPointTest, EventForwardedToCompositor);
  FRIEND_TEST_ALL_PREFIXES(DelegatedInkPointTest,
                           MojoInterfaceReboundOnDisconnect);
  FRIEND_TEST_ALL_PREFIXES(NoCompositingRenderWidgetHostViewBrowserTest,
                           NoFallbackAfterHiddenNavigationFails);
  FRIEND_TEST_ALL_PREFIXES(NoCompositingRenderWidgetHostViewBrowserTest,
                           NoFallbackIfSwapFailedBeforeNavigation);

  void SynchronizeVisualProperties();

  // Generates the most current set of ScreenInfos from the current set of
  // displays in the system for use in UpdateScreenInfo.
  display::ScreenInfos GetNewScreenInfosForUpdate();

  // Called when display properties that need to be synchronized with the
  // renderer process changes. This method is called before notifying
  // RenderWidgetHostImpl in order to allow the view to allocate a new
  // LocalSurfaceId.
  virtual void OnSynchronizedDisplayPropertiesChanged(bool rotation = false) {}

  // Transforms |point| from |original_view| coord space to |target_view| coord
  // space. Result is stored in |transformed_point|. Returns true if the
  // transform is successful, false otherwise.
  bool TransformPointToTargetCoordSpace(RenderWidgetHostViewBase* original_view,
                                        RenderWidgetHostViewBase* target_view,
                                        const gfx::PointF& point,
                                        gfx::PointF* transformed_point) const;

  // Helper function to return whether the current background color is fully
  // opaque.
  bool IsBackgroundColorOpaque();

  bool view_stopped_flinging_for_test() const {
    return view_stopped_flinging_for_test_;
  }

  base::ObserverList<RenderWidgetHostViewBaseObserver>::Unchecked observers_;

  absl::optional<blink::WebGestureEvent> pending_touchpad_pinch_begin_;

  // True when StopFlingingIfNecessary() calls StopFling().
  bool view_stopped_flinging_for_test_ = false;

  bool is_evicted_ = false;

  base::WeakPtrFactory<RenderWidgetHostViewBase> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

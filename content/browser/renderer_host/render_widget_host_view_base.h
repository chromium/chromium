// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/host/hit_test/hit_test_query.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/common/screen_info.h"
#include "content/public/common/widget_type.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/interfaces/hit_test/hit_test_region_list.mojom.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/accessibility/ax_tree_id_registry.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

#if defined(USE_AURA)
#include "base/containers/flat_map.h"
#include "content/common/render_widget_window_tree_client_factory.mojom.h"
#include "services/ws/public/mojom/window_tree.mojom.h"
#endif

struct WidgetHostMsg_SelectionBounds_Params;

namespace base {
class UnguessableToken;
}

namespace cc {
struct BeginFrameAck;
}  // namespace cc

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace ui {
enum class DomCode;
class LatencyInfo;
class Layer;
struct DidOverscrollParams;
}

namespace viz {
class SurfaceHittestDelegate;
}

namespace content {

class BrowserAccessibilityDelegate;
class BrowserAccessibilityManager;
class CursorManager;
class MouseWheelPhaseHandler;
class RenderWidgetHostImpl;
class RenderWidgetHostViewBaseObserver;
class SyntheticGestureTarget;
class TextInputManager;
class TouchSelectionControllerClientManager;
class WebContentsAccessibility;
class WebCursor;
class DelegatedFrameHost;
struct TextInputState;

// Basic implementation shared by concrete RenderWidgetHostView subclasses.
class CONTENT_EXPORT RenderWidgetHostViewBase
    : public RenderWidgetHostView,
      public RenderFrameMetadataProvider::Observer {
 public:
  using CreateCompositorFrameSinkCallback =
      base::OnceCallback<void(const viz::FrameSinkId&)>;

  ~RenderWidgetHostViewBase() override;

  float current_device_scale_factor() const {
    return current_device_scale_factor_;
  }

  // Returns the focused RenderWidgetHost inside this |view|'s RWH.
  RenderWidgetHostImpl* GetFocusedWidget() const;

  // RenderWidgetHostView implementation.
  RenderWidgetHost* GetRenderWidgetHost() const final;
  ui::TextInputClient* GetTextInputClient() override;
  void WasUnOccluded() override {}
  void WasOccluded() override {}
  void SetIsInVR(bool is_in_vr) override;
  base::string16 GetSelectedText() override;
  base::string16 GetSurroundingText() override;
  gfx::Range GetSelectedRange() override;
  size_t GetOffsetForSurroundingText() override;
  bool IsMouseLocked() override;
  bool LockKeyboard(base::Optional<base::flat_set<ui::DomCode>> codes) override;
  void SetBackgroundColor(SkColor color) override;
  base::Optional<SkColor> GetBackgroundColor() const override;
  void UnlockKeyboard() override;
  bool IsKeyboardLocked() override;
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap() override;
  gfx::Size GetVisibleViewportSize() const override;
  void SetInsets(const gfx::Insets& insets) override;
  bool IsSurfaceAvailableForCopy() const override;
  void CopyFromSurface(
      const gfx::Rect& src_rect,
      const gfx::Size& output_size,
      base::OnceCallback<void(const SkBitmap&)> callback) override;
  std::unique_ptr<viz::ClientFrameSinkVideoCapturer> CreateVideoCapturer()
      override;
  void FocusedNodeTouched(bool editable) override;
  void GetScreenInfo(ScreenInfo* screen_info) const override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  bool IsScrollOffsetAtTop() const override;
  float GetDeviceScaleFactor() const final;
  TouchSelectionControllerClientManager*
  GetTouchSelectionControllerClientManager() override;

  // This only needs to be overridden by RenderWidgetHostViewBase subclasses
  // that handle content embedded within other RenderWidgetHostViews.
  gfx::PointF TransformPointToRootCoordSpaceF(
      const gfx::PointF& point) override;
  gfx::PointF TransformRootPointToViewCoordSpace(
      const gfx::PointF& point) override;

  // RenderFrameMetadataProvider::Observer
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation() override;
  void OnRenderFrameSubmission() override;
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override;

  virtual void UpdateIntrinsicSizingInfo(
      const blink::WebIntrinsicSizingInfo& sizing_info);

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

  // Return a value that is incremented each time the renderer swaps a new frame
  // to the view.
  uint32_t RendererFrameNumber();

  // Called each time the RenderWidgetHost receives a new frame for display from
  // the renderer.
  void DidReceiveRendererFrame();

  // Notification that a resize or move session ended on the native widget.
  void UpdateScreenInfo(gfx::NativeView view);

  // Tells if the display property (work area/scale factor) has
  // changed since the last time.
  bool HasDisplayPropertyChanged(gfx::NativeView view);

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
  virtual void SelectionChanged(const base::string16& text,
                                size_t offset,
                                const gfx::Range& range);

  // The requested size of the renderer. May differ from GetViewBounds().size()
  // when the view requires additional throttling.
  virtual gfx::Size GetRequestedRendererSize() const;

  // Returns the current capture sequence number.
  virtual uint32_t GetCaptureSequenceNumber() const;

  // The size of the view's backing surface in non-DPI-adjusted pixels.
  virtual gfx::Size GetCompositorViewportPixelSize() const;

  // Whether or not the renderer's viewport size should be shrunk by the height
  // of the URL-bar.
  virtual bool DoBrowserControlsShrinkRendererSize() const;

  // The height of the URL-bar browser controls.
  virtual float GetTopControlsHeight() const;

  // The height of the bottom bar.
  virtual float GetBottomControlsHeight() const;

  // If mouse wheels can only specify the number of ticks of some static
  // multiplier constant, this method returns that constant (in DIPs). If mouse
  // wheels can specify an arbitrary delta this returns 0.
  virtual int GetMouseWheelMinimumGranularity() const;

  // Called prior to forwarding input event messages to the renderer, giving
  // the view a chance to perform in-process event filtering or processing.
  // Return values of |NOT_CONSUMED| or |UNKNOWN| will result in |input_event|
  // being forwarded.
  virtual InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& input_event);

  // Allows a root RWHV to filter gesture events in a child.
  // TODO(mcnee): Remove once both callers are removed, following
  // scroll-latching being enabled and BrowserPlugin being removed.
  // crbug.com/751782
  virtual InputEventAckState FilterChildGestureEvent(
      const blink::WebGestureEvent& gesture_event);

  virtual void WheelEventAck(const blink::WebMouseWheelEvent& event,
                             InputEventAckState ack_result);

  virtual void GestureEventAck(const blink::WebGestureEvent& event,
                               InputEventAckState ack_result);

  // Create a platform specific SyntheticGestureTarget implementation that will
  // be used to inject synthetic input events.
  virtual std::unique_ptr<SyntheticGestureTarget>
  CreateSyntheticGestureTarget();

  // Create a BrowserAccessibilityManager for a frame in this view.
  // If |for_root_frame| is true, creates a BrowserAccessibilityManager
  // suitable for the root frame, which may be linked to its native
  // window container.
  virtual BrowserAccessibilityManager* CreateBrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate, bool for_root_frame);

  virtual void AccessibilityShowMenu(const gfx::Point& point);
  virtual gfx::Point AccessibilityOriginInScreen(const gfx::Rect& bounds);
  virtual gfx::AcceleratedWidget AccessibilityGetAcceleratedWidget();
  virtual gfx::NativeViewAccessible AccessibilityGetNativeViewAccessible();
  virtual void SetMainFrameAXTreeID(ui::AXTreeID id) {}
  // Informs that the focused DOM node has changed.
  virtual void FocusedNodeChanged(bool is_editable_node,
                                  const gfx::Rect& node_bounds_in_screen) {}

  // This method is called by RenderWidgetHostImpl when a new
  // RendererCompositorFrameSink is created in the renderer. The view is
  // expected not to return resources belonging to the old
  // RendererCompositorFrameSink after this method finishes.
  virtual void DidCreateNewRendererCompositorFrameSink(
      viz::mojom::CompositorFrameSinkClient*
          renderer_compositor_frame_sink) = 0;

  // This is called by the RenderWidgetHostImpl to provide a new compositor
  // frame that was received from the renderer process. if Viz service hit
  // testing is enabled then a HitTestRegionList provides hit test data
  // that is used for routing input events.
  // TODO(kenrb): When Viz service is enabled on all platforms,
  // |hit_test_region_list| should stop being an optional argument.
  virtual void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list) = 0;

  virtual void OnDidNotProduceFrame(const viz::BeginFrameAck& ack) {}

  // This method exists to allow removing of displayed graphics, after a new
  // page has been loaded, to prevent the displayed URL from being out of sync
  // with what is visible on screen.
  virtual void ClearCompositorFrame() = 0;

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
  virtual void ProcessAckedTouchEvent(const TouchEventWithLatencyInfo& touch,
                                      InputEventAckState ack_result);

  virtual void DidOverscroll(const ui::DidOverscrollParams& params) {}

  virtual void DidStopFlinging() {}

  // Returns the ID associated with the CompositorFrameSink of this view.
  virtual const viz::FrameSinkId& GetFrameSinkId() const = 0;

  // Returns the LocalSurfaceId allocated by the parent client for this view.
  // TODO(fsamuel): Return by const ref.
  virtual const viz::LocalSurfaceId& GetLocalSurfaceId() const = 0;

  // Returns the time at which the viz::LocalSurfaceId was allocated by the
  // parent client for this view.
  virtual base::TimeTicks GetLocalSurfaceIdAllocationTime() const;

  // When there are multiple RenderWidgetHostViews for a single page, input
  // events need to be targeted to the correct one for handling. The following
  // methods are invoked on the RenderWidgetHostView that should be able to
  // properly handle the event (i.e. it has focus for keyboard events, or has
  // been identified by hit testing mouse, touch or gesture events).
  // |out_query_renderer| is set if there is low confidence in the hit test
  // result which means that renderer process hit testing could potentially
  // give a different result. In that case the returned FrameSinkId and
  // transformed point should be ignored.
  virtual viz::FrameSinkId FrameSinkIdAtPoint(
      viz::SurfaceHittestDelegate* delegate,
      const gfx::PointF& point,
      gfx::PointF* transformed_point,
      bool* out_query_renderer);

  virtual void InjectTouchEvent(const blink::WebTouchEvent& event,
                                const ui::LatencyInfo& latency) {}

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
  bool TransformPointToLocalCoordSpace(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point,
      viz::EventSource source = viz::EventSource::ANY);

  // This is deprecated, and will be removed once Viz hit-test is the default.
  virtual bool TransformPointToLocalCoordSpaceLegacy(
      const gfx::PointF& point,
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
      gfx::PointF* transformed_point,
      viz::EventSource source = viz::EventSource::ANY);

  // On success, returns true and modifies |*transform| to represent the
  // transformation mapping a point in the coordinate space of this view
  // into the coordinate space of the target view.
  // On Failure, returns false, and leaves |*transform| unchanged.
  // This function will fail if viz hit testing is not enabled, or if either
  // this view or the target view has no current FrameSinkId. The latter may
  // happen if either view is not currently visible in the viewport.
  // This function is useful if there are multiple points to transform between
  // the same two views.
  bool GetTransformToViewCoordSpace(RenderWidgetHostViewBase* target_view,
                                    gfx::Transform* transform);

  // TODO(kenrb, wjmaclean): This is a temporary subclass identifier for
  // RenderWidgetHostViewGuests that is needed for special treatment during
  // input event routing. It can be removed either when RWHVGuests properly
  // support direct mouse event routing, or when RWHVGuest is removed
  // entirely, which comes first.
  virtual bool IsRenderWidgetHostViewGuest();

  // Subclass identifier for RenderWidgetHostViewChildFrames. This is useful
  // to be able to know if this RWHV is embedded within another RWHV. If
  // other kinds of embeddable RWHVs are created, this should be renamed to
  // a more generic term -- in which case, static casts to RWHVChildFrame will
  // need to also be resolved.
  virtual bool IsRenderWidgetHostViewChildFrame();

  // Notify the View that a screen rect update is being sent to the
  // RenderWidget. Related platform-specific updates can be sent from here.
  virtual void WillSendScreenRects() {}

  // Returns true if the current view is in virtual reality mode.
  virtual bool IsInVR() const;

  // Obtains the root window FrameSinkId.
  virtual viz::FrameSinkId GetRootFrameSinkId();

  // Returns the SurfaceId currently in use by the renderer to submit compositor
  // frames.
  virtual viz::SurfaceId GetCurrentSurfaceId() const = 0;

  // Returns true if this view's size have been initialized.
  virtual bool HasSize() const;

  // Informs the view that the assocaited InterstitialPage was attached.
  virtual void OnInterstitialPageAttached() {}

  // Tells the view that the assocaited InterstitialPage will going away (but is
  // not yet destroyed, as InterstitialPage destruction is asynchronous). The
  // view may use this notification to clean up associated resources. This
  // should be called before the WebContents is fully destroyed.
  virtual void OnInterstitialPageGoingAway() {}

  //----------------------------------------------------------------------------
  // The following methods are related to IME.
  // TODO(ekaramad): Most of the IME methods should not stay virtual after IME
  // is implemented for OOPIF. After fixing IME, mark the corresponding methods
  // non-virtual (https://crbug.com/578168).

  // Updates the state of the input method attached to the view.
  virtual void TextInputStateChanged(const TextInputState& text_input_state);

  // Cancel the ongoing composition of the input method attached to the view.
  virtual void ImeCancelComposition();

  // Notifies the view that the renderer selection bounds has changed.
  // Selection bounds are described as a focus bound which is the current
  // position of caret on the screen, as well as the anchor bound which is the
  // starting position of the selection. The coordinates are with respect to
  // RenderWidget's window's origin. Focus and anchor bound are represented as
  // gfx::Rect.
  virtual void SelectionBoundsChanged(
      const WidgetHostMsg_SelectionBounds_Params& params);

  // Updates the range of the marked text in an IME composition.
  virtual void ImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds);

  //----------------------------------------------------------------------------
  // The following pure virtual methods are implemented by derived classes.

  // Perform all the initialization steps necessary for this object to represent
  // a popup (such as a <select> dropdown), then shows the popup at |pos|.
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& bounds) = 0;

  // Perform all the initialization steps necessary for this object to represent
  // a full screen window.
  // |reference_host_view| is the view associated with the creating page that
  // helps to position the full screen widget on the correct monitor.
  virtual void InitAsFullscreen(RenderWidgetHostView* reference_host_view) = 0;

  // Sets the cursor for this view to the one associated with the specified
  // cursor_type.
  virtual void UpdateCursor(const WebCursor& cursor) = 0;

  // Changes the cursor that is displayed on screen. This may or may not match
  // the current cursor's view which was set by UpdateCursor.
  virtual void DisplayCursor(const WebCursor& cursor);

  // Views that manage cursors for window return a CursorManager. Other views
  // return nullptr.
  virtual CursorManager* GetCursorManager();

  // Indicates whether the page has finished loading.
  virtual void SetIsLoading(bool is_loading) = 0;

  // Notifies the View that the renderer has ceased to exist.
  virtual void RenderProcessGone(base::TerminationStatus status,
                                 int error_code) = 0;

  // Tells the View to destroy itself.
  virtual void Destroy();

  // Tells the View that the tooltip text for the current mouse position over
  // the page has changed.
  virtual void SetTooltipText(const base::string16& tooltip_text) = 0;

  // Displays the requested tooltip on the screen.
  virtual void DisplayTooltipText(const base::string16& tooltip_text) {}

  // Transforms |point| to be in the coordinate space of browser compositor's
  // surface. This is in DIP.
  virtual void TransformPointToRootSurface(gfx::PointF* point);

  // Gets the bounds of the top-level window, in screen coordinates.
  virtual gfx::Rect GetBoundsInRootWindow() = 0;

  // Called by the WebContentsImpl when a user tries to navigate a new page on
  // main frame.
  virtual void OnDidNavigateMainFrameToNewPage();

  // Called by WebContentsImpl to notify the view about a change in visibility
  // of context menu. The view can then perform platform specific tasks and
  // changes.
  virtual void SetShowingContextMenu(bool showing) {}

  virtual void OnAutoscrollStart();

  // Returns the associated RenderWidgetHostImpl.
  RenderWidgetHostImpl* host() const { return host_; }

  // Process swap messages sent before |frame_token| in RenderWidgetHostImpl.
  void OnFrameTokenChangedForView(uint32_t frame_token);

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

  bool is_fullscreen() { return is_fullscreen_; }

  void set_web_contents_accessibility(WebContentsAccessibility* wcax) {
    web_contents_accessibility_ = wcax;
  }

  void set_is_currently_scrolling_viewport(
      bool is_currently_scrolling_viewport) {
    is_currently_scrolling_viewport_ = is_currently_scrolling_viewport;
  }

  bool is_currently_scrolling_viewport() {
    return is_currently_scrolling_viewport_;
  }
#if defined(USE_AURA)
  void EmbedChildFrameRendererWindowTreeClient(
      RenderWidgetHostViewBase* root_view,
      int routing_id,
      ws::mojom::WindowTreeClientPtr renderer_window_tree_client);
  void OnChildFrameDestroyed(int routing_id);
#endif

#if defined(OS_MACOSX)
  // Use only for resize on macOS. Returns true if there is not currently a
  // frame of the view's size being displayed.
  virtual bool ShouldContinueToPauseForFrame();

  // Specify a ui::Layer into which the renderer's content should be
  // composited. If nullptr is specified, then this layer will create a
  // separate ui::Compositor as needed (e.g, for tab capture).
  virtual void SetParentUiLayer(ui::Layer* parent_ui_layer);
#endif

  virtual void DidNavigate();

  // Called when the RenderWidgetHostImpl has be initialized.
  virtual void OnRenderWidgetInit() {}

 protected:
  explicit RenderWidgetHostViewBase(RenderWidgetHost* host);

  // SetContentBackgroundColor is called when the render wants to  update the
  // view's background color.
  void SetContentBackgroundColor(SkColor color);
  void NotifyObserversAboutShutdown();

  virtual MouseWheelPhaseHandler* GetMouseWheelPhaseHandler();

  // Applies background color without notifying the RenderWidget about
  // opaqueness changes. This allows us to, when navigating to a new page,
  // transfer this color to that page. This allows us to pass this background
  // color to new views on navigation.
  virtual void UpdateBackgroundColor() = 0;

#if defined(USE_AURA)
  virtual void ScheduleEmbed(
      ws::mojom::WindowTreeClientPtr client,
      base::OnceCallback<void(const base::UnguessableToken&)> callback);

  ws::mojom::WindowTreeClientPtr GetWindowTreeClientFromRenderer();
#endif

  // If |event| is a touchpad pinch or double tap event for which we've sent a
  // synthetic wheel event, forward the |event| to the renderer, subject to
  // |ack_result| which is the ACK result of the synthetic wheel.
  virtual void ForwardTouchpadZoomEventIfNecessary(
      const blink::WebGestureEvent& event,
      InputEventAckState ack_result);

  virtual bool HasFallbackSurface() const;

  // Cached bool to test if the VizHitTesting feature is enabled.
  const bool use_viz_hit_test_;

  // The model object. Members will become private when
  // RenderWidgetHostViewGuest is removed.
  RenderWidgetHostImpl* host_;

  // Is this a fullscreen view?
  bool is_fullscreen_ = false;

  // Whether this view is a frame or a popup.
  WidgetType widget_type_ = WidgetType::kFrame;

  // Indicates whether keyboard lock is active for this view.
  bool keyboard_locked_ = false;

  // Indicates whether the scroll offset of the root layer is at top, i.e.,
  // whether scroll_offset.y() == 0.
  bool is_scroll_offset_at_top_ = true;

  // The scale factor of the display the renderer is currently on.
  float current_device_scale_factor_ = 0;

  // The color space of the display the renderer is currently on.
  gfx::ColorSpace current_display_color_space_;

  // The orientation of the display the renderer is currently on.
  display::Display::Rotation current_display_rotation_ =
      display::Display::ROTATE_0;

  // A reference to current TextInputManager instance this RWHV is registered
  // with. This is initially nullptr until the first time the view calls
  // GetTextInputManager(). It also becomes nullptr when TextInputManager is
  // destroyed before the RWHV is destroyed.
  TextInputManager* text_input_manager_ = nullptr;

  // The background color used in the current renderer.
  base::Optional<SkColor> content_background_color_;

  // The default background color used before getting the
  // |content_background_color|.
  base::Optional<SkColor> default_background_color_;

  WebContentsAccessibility* web_contents_accessibility_ = nullptr;

  bool is_currently_scrolling_viewport_ = false;

 private:
  void SynchronizeVisualProperties();

#if defined(USE_AURA)
  void OnDidScheduleEmbed(int routing_id,
                          int embed_id,
                          const base::UnguessableToken& token);
#endif

  // Called when display properties that need to be synchronized with the
  // renderer process changes. This method is called before notifying
  // RenderWidgetHostImpl in order to allow the view to allocate a new
  // LocalSurfaceId.
  virtual void OnSynchronizedDisplayPropertiesChanged() {}

  // Transforms |point| from |original_view| coord space to |target_view| coord
  // space. Result is stored in |transformed_point|. Returns true if the
  // transform is successful, false otherwise.
  bool TransformPointToTargetCoordSpace(RenderWidgetHostViewBase* original_view,
                                        RenderWidgetHostViewBase* target_view,
                                        const gfx::PointF& point,
                                        gfx::PointF* transformed_point,
                                        viz::EventSource source) const;

  // Used to transform |point| when Viz hit-test is enabled.
  // TransformPointToLocalCoordSpaceLegacy is used in non-Viz hit-testing.
  bool TransformPointToLocalCoordSpaceViz(
      const gfx::PointF& point,
      const viz::SurfaceId& original_surface,
      gfx::PointF* transformed_point,
      viz::EventSource source);

  gfx::Rect current_display_area_;

  uint32_t renderer_frame_number_ = 0;

  base::ObserverList<RenderWidgetHostViewBaseObserver>::Unchecked observers_;

#if defined(USE_AURA)
  mojom::RenderWidgetWindowTreeClientPtr render_widget_window_tree_client_;

  int next_embed_id_ = 0;
  // Maps from routing_id to embed-id. The |routing_id| is the id supplied to
  // EmbedChildFrameRendererWindowTreeClient() and the embed-id a unique id
  // generate at the time EmbedChildFrameRendererWindowTreeClient() was called.
  // This is done to ensure when OnDidScheduleEmbed() is received another call
  // too EmbedChildFrameRendererWindowTreeClient() did not come in.
  base::flat_map<int, int> pending_embeds_;
#endif

  base::Optional<blink::WebGestureEvent> pending_touchpad_pinch_begin_;

  base::WeakPtrFactory<RenderWidgetHostViewBase> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewBase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_BASE_H_

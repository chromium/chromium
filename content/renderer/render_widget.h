// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_H_
#define CONTENT_RENDERER_RENDER_WIDGET_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/input/overscroll_behavior.h"
#include "cc/input/touch_action.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/edit_command.h"
#include "content/common/tab_switch_time_recorder.h"
#include "content/common/widget.mojom.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/compositor/layer_tree_view_delegate.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/renderer/render_widget_mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_screen_metrics_emulator_delegate.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

#if defined(OS_MACOSX)
#include "content/renderer/text_input_client_observer.h"
#endif

namespace IPC {
class SyncMessageFilter;
}

namespace blink {
namespace scheduler {
class WebRenderWidgetSchedulingState;
}
struct WebDeviceEmulationParams;
class WebDragData;
class WebFrameWidget;
class WebInputMethodController;
class WebLocalFrame;
class WebMouseEvent;
class WebPagePopup;
}  // namespace blink

namespace cc {
struct ApplyViewportChangesArgs;
class SwapPromise;
}

namespace gfx {
class ColorSpace;
struct PresentationFeedback;
class Range;
}

namespace ui {
struct DidOverscrollParams;
}

namespace content {
class BrowserPlugin;
class CompositorDependencies;
class FrameSwapMessageQueue;
class ImeEventGuard;
class LayerTreeView;
class MainThreadEventQueue;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderViewImpl;
class RenderWidgetDelegate;
class RenderWidgetScreenMetricsEmulator;
class WidgetInputHandlerManager;
struct VisualProperties;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Fullscreen mode (RenderWidgetFullScreen)
// - Popup "menus" (like the color chooser and date picker)
// - Widgets for frames (the main frame, and subframes due to out-of-process
//   iframe support)
//
// Background info:
// OOPIF causes webpages to be renderered by multiple renderers. Each renderer
// has one instance of a RenderViewImpl, which represents page state shared by
// each renderer. The frame tree is mirrored across each renderer. Local nodes
// are represented by RenderFrame, and remote nodes are represented by
// RenderFrameProxy. Each local root has a corresponding RenderWidget. This
// RenderWidget is used to route input and graphical output between the browser
// and the renderer.
//
// For legacy reasons, each RenderViewImpl also has a RenderWidget. The
// RenderViewImpl hosting the local main frame legitimately needs a
// RenderWidget, since this is a local root. To reduce complexity, we'd like the
// RenderFrame to own the RenderWidget, which it already does for local root
// subframes. This is tracked by https://crbug.com/419087.
//
// RenderViewImpls with a proxy main frame still own a RenderWidget, even though
// the RenderWidget is not used by content/renderer or blink. This is because
// the browser side semantics for RenderWidgetHost, RenderViewHost and
// RenderFrameHost are still entangled. If we destroyed the RenderWidget
// without destroying the corresponding RenderWidgetHost, we'd break IPC
// channels that would not be re-established when recreating the RenderWidget.
// These RenderWidgets are called "undead", and they should never be used.
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      public blink::WebPagePopupClient,  // Is-a WebWidgetClient also.
      public mojom::Widget,
      public LayerTreeViewDelegate,
      public RenderWidgetInputHandlerDelegate,
      public RenderWidgetScreenMetricsEmulatorDelegate,
      public MainThreadEventQueueClient {
 public:
  RenderWidget(int32_t widget_routing_id,
               CompositorDependencies* compositor_deps,
               blink::mojom::DisplayMode display_mode,
               bool is_undead,
               bool hidden,
               bool never_visible,
               mojo::PendingReceiver<mojom::Widget> widget_receiver);

  ~RenderWidget() override;

  using ShowCallback =
      base::OnceCallback<void(RenderWidget* widget_to_show,
                              blink::WebNavigationPolicy policy,
                              const gfx::Rect& initial_rect)>;

  // Time-To-First-Active-Paint(TTFAP) type
  enum {
    TTFAP_AFTER_PURGED,
    TTFAP_5MIN_AFTER_BACKGROUNDED,
  };

  // Convenience type for creation method taken by InstallCreateForFrameHook().
  // The method signature matches the RenderWidget constructor.
  using CreateRenderWidgetFunction = std::unique_ptr<RenderWidget> (*)(
      int32_t,
      CompositorDependencies*,
      blink::mojom::DisplayMode display_mode,
      bool is_undead,
      bool never_visible,
      mojo::PendingReceiver<mojom::Widget> widget_receiver);
  // Overrides the implementation of CreateForFrame() function below. Used by
  // web tests to return a partial fake of RenderWidget.
  static void InstallCreateForFrameHook(
      CreateRenderWidgetFunction create_widget);

  // Creates a RenderWidget that is meant to be associated with a RenderFrame.
  // Testing infrastructure, such as test_runner, can override this function
  // by calling InstallCreateForFrameHook().
  static std::unique_ptr<RenderWidget> CreateForFrame(
      int32_t widget_routing_id,
      CompositorDependencies* compositor_deps,
      blink::mojom::DisplayMode display_mode,
      bool is_undead,
      bool never_visible);

  // Creates a RenderWidget for a popup. This is separate from CreateForFrame()
  // because popups do not not need to be faked out.
  // A RenderWidget popup is owned by the browser process. The object will be
  // destroyed by the WidgetMsg_Close message. The object can request its own
  // destruction via ClosePopupWidgetSoon().
  static RenderWidget* CreateForPopup(
      int32_t widget_routing_id,
      CompositorDependencies* compositor_deps,
      blink::mojom::DisplayMode display_mode,
      bool hidden,
      bool never_visible,
      mojo::PendingReceiver<mojom::Widget> widget_receiver);

  // Initialize a new RenderWidget for a popup. The |show_callback| is called
  // when RenderWidget::Show() happens. The |opener_widget| is the local root
  // of the frame that is opening the popup.
  void InitForPopup(ShowCallback show_callback,
                    RenderWidget* opener_widget,
                    blink::WebPagePopup* web_page_popup,
                    const ScreenInfo& screen_info);

  // Initialize a new RenderWidget for pepper fullscreen. The |show_callback| is
  // called when RenderWidget::Show() happens.
  void InitForPepperFullscreen(ShowCallback show_callback,
                               blink::WebWidget* web_widget,
                               const ScreenInfo& screen_info);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a main frame. When WebFrameWidget
  // is given, a ScreenInfo must be also.
  void InitForMainFrame(ShowCallback show_callback,
                        blink::WebFrameWidget* web_frame_widget,
                        const ScreenInfo* screen_info);

  // Initialize (or re-initialize) a main frame RenderWidget that has been
  // revived from undead state.
  void InitForRevivedMainFrame(blink::WebFrameWidget* web_frame_widget,
                               const ScreenInfo& screen_info);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a local root, but not the main
  // frame.
  void InitForChildLocalRoot(blink::WebFrameWidget* web_frame_widget,
                             const ScreenInfo& screen_info);

  // Sets a delegate to handle certain RenderWidget operations that need an
  // escape to the RenderView.
  void set_delegate(RenderWidgetDelegate* delegate) {
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  RenderWidgetDelegate* delegate() const { return delegate_; }

  // Returns the RenderWidget for the given routing ID.
  static RenderWidget* FromRoutingID(int32_t routing_id);

  // Closes a RenderWidget that was created by |CreateForFrame|. Ownership is
  // passed into this object to asynchronously delete itself.
  void CloseForFrame(std::unique_ptr<RenderWidget> widget);

  int32_t routing_id() const { return routing_id_; }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }

  // This can return nullptr while the RenderWidget is closing. When for_frame()
  // is true, the widget returned is a blink::WebFrameWidget.
  // TODO(crbug.com/419087): The main frame RenderWidget will also return
  // nullptr while the main frame is remote.
  blink::WebWidget* GetWebWidget() const { return webwidget_; }

  // Returns the current instance of WebInputMethodController which is to be
  // used for IME related tasks. This instance corresponds to the one from
  // focused frame and can be nullptr.
  blink::WebInputMethodController* GetInputMethodController() const;

  const gfx::Size& size() const { return size_; }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::mojom::DisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  bool has_host_context_menu_location() const {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() const {
    return host_context_menu_location_;
  }
  const gfx::Size& visible_viewport_size() const {
    return visible_viewport_size_;
  }

  // Sets whether this RenderWidget should be moved into or out of an undead
  // state. This state is used for the RenderWidget attached to a RenderViewImpl
  // for its main frame, when there is no local main frame present.
  // In this case, the RenderWidget can't be deleted currently but should
  // otherwise act as if it is dead. Only whitelisted new IPC messages will be
  // sent, and it does no compositing. The process is free to exit when there
  // are no other non-undead RenderWidgets.
  void SetIsUndead(bool is_undead);

  // A main frame RenderWidget is made undead instead of being deleted. Then
  // when a provisional frame is created, the RenderWidget is recycled and
  // attached to it. Code that wants to prevent using the RenderWidget once it
  // has been made undead would race with a new provisional frame attaching it,
  // so should check for both states via this method instead.
  bool IsUndeadOrProvisional() { return is_undead_ || IsForProvisionalFrame(); }

  // Manage edit commands to be used for the next keyboard event.
  const EditCommands& edit_commands() const { return edit_commands_; }
  void SetEditCommandForNextKeyEvent(const std::string& name,
                                     const std::string& value);
  void ClearEditCommands();

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

  // BrowserPlugins embedded by this RenderWidget register themselves here.
  // These plugins need to be notified about changes to ScreenInfo.
  void RegisterBrowserPlugin(BrowserPlugin* browser_plugin);
  void UnregisterBrowserPlugin(BrowserPlugin* browser_plugin);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // LayerTreeViewDelegate
  void ApplyViewportChanges(const cc::ApplyViewportChangesArgs& args) override;
  void RecordManipulationTypeCounts(cc::ManipulationInfo info) override;
  void SendOverscrollEventFromImplSide(
      const gfx::Vector2dF& overscroll_delta,
      cc::ElementId scroll_latched_element_id) override;
  void SendScrollEndEventFromImplSide(
      cc::ElementId scroll_latched_element_id) override;
  void BeginMainFrame(base::TimeTicks frame_time) override;
  void OnDeferMainFrameUpdatesChanged(bool) override;
  void OnDeferCommitsChanged(bool) override;
  void DidBeginMainFrame() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void WillCommitCompositorFrame() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  std::unique_ptr<cc::BeginMainFrameMetrics> GetBeginMainFrameMetrics()
      override;

  void BeginUpdateLayers() override;
  void EndUpdateLayers() override;
  void UpdateVisualState() override;
  void WillBeginCompositorFrame() override;

  // RenderWidgetInputHandlerDelegate
  void FocusChangeComplete() override;
  void ObserveGestureEventAndResult(
      const blink::WebGestureEvent& gesture_event,
      const gfx::Vector2dF& unused_delta,
      const cc::OverscrollBehavior& overscroll_behavior,
      bool event_processed) override;

  void OnDidHandleKeyEvent() override;
  void OnDidOverscroll(const ui::DidOverscrollParams& params) override;
  void SetInputHandler(RenderWidgetInputHandler* input_handler) override;
  void ShowVirtualKeyboard() override;
  void UpdateTextInputState() override;
  void ClearTextInputState() override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;

  // RenderWidgetScreenMetricsEmulatorDelegate
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;
  void SetScreenInfoAndSize(const ScreenInfo& screen_info,
                            const gfx::Size& widget_size,
                            const gfx::Size& visible_viewport_size) override;
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect) override;

  // blink::WebWidgetClient
  void SetLayerTreeMutator(std::unique_ptr<cc::LayerTreeMutator>) override;
  void SetPaintWorkletLayerPainterClient(
      std::unique_ptr<cc::PaintWorkletLayerPainter>) override;
  void SetRootLayer(scoped_refptr<cc::Layer> layer) override;
  void ScheduleAnimation() override;
  void SetShowFPSCounter(bool show) override;
  void SetShowLayoutShiftRegions(bool) override;
  void SetShowPaintRects(bool) override;
  void SetShowDebugBorders(bool) override;
  void SetShowScrollBottleneckRects(bool) override;
  void SetShowHitTestBorders(bool) override;
  void SetBackgroundColor(SkColor color) override;
  void IntrinsicSizingInfoChanged(
      const blink::WebIntrinsicSizingInfo&) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidChangeCursor(const blink::WebCursorInfo&) override;
  void AutoscrollStart(const blink::WebFloatPoint& point) override;
  void AutoscrollFling(const blink::WebFloatSize& velocity) override;
  void AutoscrollEnd() override;
  void ClosePopupWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  blink::WebScreenInfo GetScreenInfo() override;
  blink::WebRect WindowRect() override;
  blink::WebRect ViewRect() override;
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void SetWindowRect(const blink::WebRect&) override;
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void DidOverscroll(const blink::WebFloatSize& overscroll_delta,
                     const blink::WebFloatSize& accumulated_overscroll,
                     const blink::WebFloatPoint& position,
                     const blink::WebFloatSize& velocity) override;
  void InjectGestureScrollEvent(
      blink::WebGestureDevice device,
      const blink::WebFloatSize& delta,
      ui::input_types::ScrollGranularity granularity,
      cc::ElementId scrollable_area_element_id,
      blink::WebInputEvent::Type injected_type) override;
  void SetOverscrollBehavior(const cc::OverscrollBehavior&) override;
  void ShowVirtualKeyboardOnElementFocus() override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertViewportToWindow(blink::WebFloatRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  bool RequestPointerLock(blink::WebLocalFrame* requester_frame,
                          bool request_unadjusted_movement) override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void StartDragging(network::mojom::ReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const SkBitmap& drag_image,
                     const gfx::Point& image_offset) override;
  void SetTouchAction(cc::TouchAction touch_action) override;
  void RequestUnbufferedInputEvents() override;
  void SetHasPointerRawUpdateEventHandlers(bool has_handlers) override;
  void SetHasTouchEventHandlers(bool has_handlers) override;
  void SetHaveScrollEventHandlers(bool have_handlers) override;
  void SetNeedsLowLatencyInput(bool) override;
  void SetNeedsUnbufferedInputForDebugger(bool) override;
  void AnimateDoubleTapZoomInMainFrame(const blink::WebPoint& point,
                                       const blink::WebRect& bounds) override;
  void ZoomToFindInPageRectInMainFrame(
      const blink::WebRect& rect_to_zoom) override;
  void RegisterSelection(const cc::LayerSelection& selection) override;
  void FallbackCursorModeLockCursor(bool left,
                                    bool right,
                                    bool up,
                                    bool down) override;
  void FallbackCursorModeSetCursorVisibility(bool visible) override;
  void SetPageScaleStateAndLimits(float page_scale_factor,
                                  bool is_pinch_gesture_active,
                                  float minimum,
                                  float maximum) override;
  void StartPageScaleAnimation(const gfx::Vector2d& destination,
                               bool use_anchor,
                               float new_page_scale,
                               base::TimeDelta duration) override;
  void ForceRecalculateRasterScales() override;
  void RequestDecode(const cc::PaintImage& image,
                     base::OnceCallback<void(bool)> callback) override;
  void NotifySwapTime(ReportTimeCallback callback) override;
  void SetEventListenerProperties(
      cc::EventListenerClass event_class,
      cc::EventListenerProperties properties) override;
  cc::EventListenerProperties EventListenerProperties(
      cc::EventListenerClass event_class) const override;
  std::unique_ptr<cc::ScopedDeferMainFrameUpdate> DeferMainFrameUpdate()
      override;
  void StartDeferringCommits(base::TimeDelta timeout) override;
  void StopDeferringCommits(cc::PaintHoldingCommitTrigger) override;
  void RequestBeginMainFrameNotExpected(bool request) override;
  int GetLayerTreeId() const override;
  void SetBrowserControlsShownRatio(float top_ratio,
                                    float bottom_ratio) override;
  void SetBrowserControlsHeight(float top_height,
                                float bottom_height,
                                bool shrink_viewport) override;
  viz::FrameSinkId GetFrameSinkId() override;

  // Returns the scale being applied to the document in blink by the device
  // emulator. Returns 1 if there is no emulation active. Use this to position
  // things when the coordinates did not come from blink, such as from the mouse
  // position.
  float GetEmulatorScale() const;

  // Registers a SwapPromise to report presentation time and possibly swap time.
  // If |swap_time_callback| is not a null callback, it would be called once
  // swap happens. |presentation_time_callback| will be called some time after
  // pixels are presented on screen. Swap time is needed only in tests and
  // production code uses |NotifySwapTime()| above which calls this one passing
  // a null callback as |swap_time_callback|.
  void NotifySwapAndPresentationTime(
      ReportTimeCallback swap_time_callback,
      ReportTimeCallback presentation_time_callback);

  // Override point to obtain that the current input method state and caret
  // position.
  ui::TextInputType GetTextInputType();

  // Sends a request to the browser to close this RenderWidget.
  void CloseWidgetSoon();

  static cc::LayerTreeSettings GenerateLayerTreeSettings(
      CompositorDependencies* compositor_deps,
      bool is_for_subframe,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);
  static cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
      const cc::ManagedMemoryPolicy& policy,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);

  LayerTreeView* layer_tree_view() const { return layer_tree_view_.get(); }
  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_; }
  WidgetInputHandlerManager* widget_input_handler_manager() {
    return widget_input_handler_manager_.get();
  }
  const RenderWidgetInputHandler& input_handler() const {
    return *input_handler_;
  }

  void SetHandlingInputEvent(bool handling_input_event);

  // Queues the IPC |message| to be sent to the browser, delaying sending until
  // the next compositor frame submission. At that time they will be sent before
  // any message from the compositor as part of submitting its frame. This is
  // used for messages that need synchronization with the compositor, but in
  // general you should use Send().
  //
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued.
  void QueueMessage(std::unique_ptr<IPC::Message> msg);

  // Handle start and finish of IME event guard.
  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  // Checks if the composition range or composition character bounds have been
  // changed. If they are changed, the new value will be sent to the browser
  // process. This method does nothing when the browser process is not able to
  // handle composition range and composition character bounds.
  // If immediate_request is true, render sends the latest composition info to
  // the browser even if the composition info is not changed.
  void UpdateCompositionInfo(bool immediate_request);

  // Override point to obtain that the current composition character bounds.
  // In the case of surrogate pairs, the character is treated as two characters:
  // the bounds for first character is actual one, and the bounds for second
  // character is zero width rectangle.
  void GetCompositionCharacterBounds(std::vector<gfx::Rect>* character_bounds);

  // Called when the Widget has changed size as a result of an auto-resize.
  void DidAutoResize(const gfx::Size& new_size);

  void DidPresentForceDrawFrame(int snapshot_id,
                                const gfx::PresentationFeedback& feedback);

  // Indicates whether this widget has focus.
  bool has_focus() const { return has_focus_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

  // When emulated, this returns the original (non-emulated) ScreenInfo.
  const ScreenInfo& GetOriginalScreenInfo() const;

  // Helper to convert |point| using ConvertWindowToViewport().
  gfx::PointF ConvertWindowPointToViewport(const gfx::PointF& point);
  gfx::Point ConvertWindowPointToViewport(const gfx::Point& point);
  void DidNavigate();

  bool auto_resize_mode() const { return auto_resize_mode_; }

  const gfx::Size& min_size_for_auto_resize() const {
    return min_size_for_auto_resize_;
  }

  const gfx::Size& max_size_for_auto_resize() const {
    return max_size_for_auto_resize_;
  }

  uint32_t capture_sequence_number() const {
    return last_capture_sequence_number_;
  }

  // Returns true if a page scale animation is active.
  bool HasPendingPageScaleAnimation() const;

  // MainThreadEventQueueClient overrides.
  bool HandleInputEvent(const blink::WebCoalescedInputEvent& input_event,
                        const ui::LatencyInfo& latency_info,
                        HandledEventCallback callback) override;
  void SetNeedsMainFrame() override;

  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  // Widget mojom overrides.
  void SetupWidgetInputHandler(
      mojo::PendingReceiver<mojom::WidgetInputHandler> receiver,
      mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) override;

  scoped_refptr<MainThreadEventQueue> GetInputEventQueue();

  void OnSetActive(bool active);
  void OnSetFocus(bool enable);
  void OnMouseCaptureLost();
  void OnCursorVisibilityChange(bool is_visible);
  void OnFallbackCursorModeToggled(bool is_on);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnImeSetComposition(
      const base::string16& text,
      const std::vector<blink::WebImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end);
  void OnImeCommitText(const base::string16& text,
                       const std::vector<blink::WebImeTextSpan>& ime_text_spans,
                       const gfx::Range& replacement_range,
                       int relative_cursor_pos);
  void OnImeFinishComposingText(bool keep_selection);

  // Called by the browser process to update text input state.
  void OnRequestTextInputStateUpdate();

  // Called by the browser process to update the cursor and composition
  // information by sending WidgetInputHandlerHost::ImeCompositionRangeChanged.
  // If |immediate_request| is true, an IPC is sent back with current state.
  // When |monitor_update| is true, then RenderWidget will send the updates
  // in each compositor frame when there are changes. Outside of compositor
  // frame updates, a change in text selection might also lead to an update for
  // composition info (when in monitor mode).
  void OnRequestCompositionUpdates(bool immediate_request,
                                   bool monitor_updates);
  void SetWidgetReceiver(mojo::PendingReceiver<mojom::Widget> receiver);

  void SetMouseCapture(bool capture);

  void UseSynchronousResizeModeForTesting(bool enable);
  void SetDeviceScaleFactorForTesting(float factor);
  void SetZoomLevelForTesting(double zoom_level);
  void ResetZoomLevelForTesting();
  void SetDeviceColorSpaceForTesting(const gfx::ColorSpace& color_space);
  void SetPageZoomLevelForTesting(double zoom_level);
  void SetWindowRectSynchronouslyForTesting(const gfx::Rect& new_window_rect);
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  // Forces a redraw and invokes the callback once the frame's been displayed
  // to the user.
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void RequestPresentation(PresentationTimeCallback callback);

  base::WeakPtr<RenderWidget> AsWeakPtr();

 protected:
  // Notify subclasses that we initiated the paint operation.
  virtual void DidInitiatePaint() {}

  // Notify subclasses that we handled OnUpdateVisualProperties.
  virtual void AfterUpdateVisualProperties() {}

  // Destroy the RenderWidget. The |widget| is the owning pointer of |this|.
  virtual void Close(std::unique_ptr<RenderWidget> widget);

 private:
  // Friend RefCounted so that the dtor can be non-public. Using this class
  // without ref-counting is an error.
  friend class base::RefCounted<RenderWidget>;

  // TODO(nasko): Temporarily friend RenderFrameImpl for WasSwappedOut(),
  // while we move frame specific code away from RenderViewImpl/RenderWidget.
  friend class RenderFrameImpl;

  // For unit tests.
  friend class InteractiveRenderWidget;
  friend class PopupRenderWidget;
  friend class QueueMessageSwapPromiseTest;
  friend class RenderWidgetTest;
  friend class RenderViewImplTest;

  // Called by InitFor*() methods on a new RenderWidget. This contains
  // initialization that occurs whether the RenderWidget is created as undead or
  // not.
  void UnconditionalInit(ShowCallback show_callback);

  // Called by InitFor*() methods when a new RenderWidget is created that is not
  // undead, or when it is being revived from undead.
  void LivingInit(blink::WebWidget* web_widget, const ScreenInfo& screen_info);
  // Initializes the compositor and dependent systems, as part of the
  // LivingInit() process.
  void InitCompositing(const ScreenInfo& screen_info);

  // If appropriate, initiates the compositor to set up IPC channels and begin
  // its scheduler. Otherwise, pauses the scheduler and tears down its IPC
  // channels.
  void StartStopCompositor();

  // Request the window to close from the renderer by sending the request to the
  // browser.
  static void DoDeferredClose(int widget_routing_id);

  // Must be called to pass updated values to blink when the widget size, the
  // visual viewport size, or the device scale factor change.
  void ResizeWebWidget();

  // Enable or disable auto-resize. This is part of
  // OnUpdateVisualProperties though tests may call to it more directly.
  void SetAutoResizeMode(bool auto_resize,
                         const gfx::Size& min_size_before_dsf,
                         const gfx::Size& max_size_before_dsf,
                         float device_scale_factor);

  // Sets the zoom level on the RenderView. This is part of
  // OnUpdateVisualProperties though tests may call to it more directly.
  void SetZoomLevel(double zoom_level);

  // Helper method to get the device_viewport_rect() from the compositor, which
  // is always in physical pixels.
  gfx::Rect CompositorViewportRect() const;

  // RenderWidget IPC message handlers.
  void OnHandleInputEvent(
      const blink::WebInputEvent* event,
      const std::vector<const blink::WebInputEvent*>& coalesced_events,
      const ui::LatencyInfo& latency_info,
      InputEventDispatchType dispatch_type);
  void OnClose();
  void OnUpdateVisualProperties(const VisualProperties& properties);
  void OnCreatingNewAck();
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  void OnWasHidden();
  void OnWasShown(base::TimeTicks show_request_timestamp,
                  bool was_evicted,
                  const base::Optional<content::RecordTabSwitchTimeRequest>&
                      record_tab_switch_time_request);
  void OnCreateVideoAck(int32_t video_id);
  void OnUpdateVideoAck(int32_t video_id);
  void OnRequestSetBoundsAck();
  void OnForceRedraw(int snapshot_id);
  void OnShowContextMenu(ui::MenuSourceType source_type,
                         const gfx::Point& location);

  void OnSetTextDirection(blink::WebTextDirection direction);
  void OnGetFPS();
  void OnUpdateScreenRects(const gfx::Rect& widget_screen_rect,
                           const gfx::Rect& window_screen_rect);
  void OnSetViewportIntersection(
      const blink::ViewportIntersectionState& intersection_state);
  void OnSetIsInert(bool);
  void OnSetInheritedEffectiveTouchAction(cc::TouchAction touch_action);
  void OnUpdateRenderThrottlingStatus(bool is_throttled,
                                      bool subtree_throttled);
  void OnDragTargetDragEnter(
      const std::vector<DropData::Metadata>& drop_meta_data,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers);
  void OnDragTargetDragOver(const gfx::PointF& client_pt,
                            const gfx::PointF& screen_pt,
                            blink::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnDragTargetDragLeave(const gfx::PointF& client_point,
                             const gfx::PointF& screen_point);
  void OnDragTargetDrop(const DropData& drop_data,
                        const gfx::PointF& client_pt,
                        const gfx::PointF& screen_pt,
                        int key_modifiers);
  void OnDragSourceEnded(const gfx::PointF& client_point,
                         const gfx::PointF& screen_point,
                         blink::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnOrientationChange();
  void OnWaitNextFrameForTests(int routing_id);

  // Sets the "hidden" state of this widget.  All modification of is_hidden_
  // should use this method so that we can properly inform the RenderThread of
  // our state.
  void SetHidden(bool hidden);

  // Returns a rect that the compositor needs to raster. For a main frame this
  // is always the entire viewport, but for out-of-process iframes this can be
  // constrained to limit overdraw.
  gfx::Rect ViewportVisibleRect();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static std::unique_ptr<cc::SwapPromise> QueueMessageImpl(
      std::unique_ptr<IPC::Message> msg,
      FrameSwapMessageQueue* frame_swap_message_queue,
      scoped_refptr<IPC::SyncMessageFilter> sync_message_filter,
      int source_frame_number);

  // Returns the range of the text that is being composed or the selection if
  // the composition does not exist.
  void GetCompositionRange(gfx::Range* range);

  // Returns true if the composition range or composition character bounds
  // should be sent to the browser process.
  bool ShouldUpdateCompositionInfo(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& bounds);

  // Override point to obtain that the current input method state about
  // composition text.
  bool CanComposeInline();

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const blink::WebRect& r);

  // Returns the WebFrameWidget associated with this RenderWidget if any.
  // Returns nullptr if GetWebWidget() returns nullptr or returns a WebWidget
  // that is not a WebFrameWidget. A WebFrameWidget only makes sense when there
  // a local root associated with it. RenderWidgetFullscreenPepper and a swapped
  // out RenderWidgets are amongst the cases where this method returns nullptr.
  blink::WebFrameWidget* GetFrameWidget() const;

  // Applies/Removes the DevTools device emulation transformation to/from a
  // screen rect.
  void ScreenRectToEmulated(gfx::Rect* screen_rect) const;
  void EmulatedToScreenRect(gfx::Rect* screen_rect) const;

  void UpdateSurfaceAndScreenInfo(
      const viz::LocalSurfaceIdAllocation& new_local_surface_id_allocation,
      const gfx::Rect& compositor_viewport_pixel_rect,
      const ScreenInfo& new_screen_info);

  // Used to force the size of a window when running web tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);

  // Determines whether or not RenderWidget should process IME events from the
  // browser. It always returns true unless there is no WebFrameWidget to
  // handle the event, or there is no page focus.
  bool ShouldHandleImeEvents() const;

  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool reply_to_request);

  gfx::ColorSpace GetRasterColorSpace() const;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Returns the focused pepper plugin, if any, inside the WebWidget. That is
  // the pepper plugin which is focused inside a frame which belongs to the
  // local root associated with this RenderWidget.
  PepperPluginInstanceImpl* GetFocusedPepperPluginInsideWidget();
#endif
  void RecordTimeToFirstActivePaint();

  // This method returns the WebLocalFrame which is currently focused and
  // belongs to the frame tree associated with this RenderWidget.
  blink::WebLocalFrame* GetFocusedWebLocalFrameInWidget() const;

  // Whether this widget is for a frame. This excludes widgets that are not for
  // a frame (eg popups, pepper), but includes both the main frame
  // (via delegate_) and subframes (via for_child_local_root_frame_).
  bool for_frame() const { return delegate_ || for_child_local_root_frame_; }

  // Whether this widget is for a frame that is currently provisional.
  bool IsForProvisionalFrame() const;

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost.
  const int32_t routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // We are responsible for destroying this object via its Close method, unless
  // the RenderWidget is associated with a RenderViewImpl through |delegate_|.
  // Becomes null once close is initiated on the RenderWidget.
  // TODO(https://crbug.com/995981): For main frame RenderWidgets associated
  // with a RenderViewImpl through |delegate_|, this is also null when the
  // RenderWidget is undead.
  blink::WebWidget* webwidget_ = nullptr;

  // The delegate for this object which is just a RenderViewImpl.
  // This member is non-null if and only if the RenderWidget is associated with
  // a RenderViewImpl.
  RenderWidgetDelegate* delegate_ = nullptr;

  // Wraps the LayerTreeHost, providing clients for it with the ability to
  // outlive RenderWidget during shutdown and keep the client pointers valid.
  std::unique_ptr<LayerTreeView> layer_tree_view_;
  // This is valid while |layer_tree_view_| is valid.
  cc::LayerTreeHost* layer_tree_host_ = nullptr;

  // Present when emulation is enabled, only in a main frame RenderWidget. Used
  // to override values given from the browser such as ScreenInfo,
  // WidgetScreenRect, WindowScreenRect, and the widget's size.
  std::unique_ptr<RenderWidgetScreenMetricsEmulator> device_emulator_;

  // When emulation is enabled, and a popup widget is opened, the popup widget
  // needs these values to move between the popup's (non-emulated) coordinates
  // and the opener widget's (emulated) coordinates. They are only valid when
  // the |opener_emulator_scale_| is non-zero.
  gfx::Point opener_widget_screen_origin_;
  gfx::Point opener_original_widget_screen_origin_;
  float opener_emulator_scale_ = 0;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // Web tests override the device scale factor in the renderer with this. We
  // store it to keep the override if the browser passes along VisualProperties
  // with the real device scale factor. A value of 0.f means this is ignored.
  float device_scale_factor_for_testing_ = 0;
  // Web tests override the zoom factor in the renderer with this. We store it
  // to keep the override if the browser passes along VisualProperties with the
  // real device scale factor. A value of -INFINITY means this is ignored.
  double zoom_level_for_testing_ = -INFINITY;

  // The size of the RenderWidget in DIPs. This may differ from the viewport
  // set in the compositor, as the viewport can be a subset of the RenderWidget
  // in such cases as:
  // - When (hiding-on-scroll) top and bottom controls are present.
  // - Rounding issues with OOPIFs (??).
  gfx::Size size_;

  // The size of the visible viewport in pixels.
  gfx::Size visible_viewport_size_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_ = false;

  // The minimum size to use for auto-resize.
  gfx::Size min_size_for_auto_resize_;

  // The maximum size to use for auto-resize.
  gfx::Size max_size_for_auto_resize_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool compositor_never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_ = false;

  // Indicates the display mode.
  blink::mojom::DisplayMode display_mode_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_ = nullptr;

  // True once Close() is called, during the self-destruction process, and to
  // verify destruction always goes through Close().
  bool closing_ = false;

  // A RenderWidget is undead if it is the RenderWidget attached to the
  // RenderViewImpl for its main frame, but there is a proxy main frame in
  // RenderViewImpl's frame tree. Since proxy frames do not have content they
  // do not need a RenderWidget.
  // This flag should never be used for RenderWidgets attached to subframes, as
  // those RenderWidgets are able to be created/deleted along with the frames,
  // unlike the main frame RenderWidget (for now).
  // TODO(419087): In this case the RenderWidget should not exist at all as
  // it has nothing to display, but since we can't destroy it without destroying
  // the RenderViewImpl, we mark it as undead instead.
  // TODO(419087): RenderWidgets attached to a provisional main frame are
  // undead, but are also semi-active, they are valid and can be used and
  // receive IPCs etc, even though they have not been swapped in. We should
  // consider a tri-state here instead.
  bool is_undead_;

  // In web tests, synchronous resizing mode may be used. Normally each widget's
  // size is controlled by IPC from the browser. In synchronous resize mode the
  // renderer controls the size directly, and IPCs from the browser must be
  // ignored. This was deprecated but then later undeprecated, so it is now
  // called unfortunate instead. See https://crbug.com/309760. When this is
  // enabled the various size properties will be controlled directly when
  // SetWindowRect() is called instead of needing a round trip through the
  // browser.
  // Note that SetWindowRectSynchronouslyForTesting() provides a secondary way
  // to control the size of the RenderWidget independently from the renderer
  // process, without the use of this mode, however it would be overridden by
  // the browser if they disagree.
  bool synchronous_resize_mode_for_testing_ = false;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_ = ui::TEXT_INPUT_TYPE_NONE;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_ = ui::TEXT_INPUT_MODE_DEFAULT;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_ = 0;

  // Indicates whether currently focused input field has next/previous focusable
  // form input field.
  int next_previous_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_ = true;

  // Stores whether the IME should always be hidden for |webwidget_|.
  bool always_hide_ime_ = false;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_ = gfx::Range::InvalidRange();

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_ = 0;
  gfx::Rect pending_window_rect_;

  // Properties of the screen hosting the RenderWidget. Rects in this structure
  // do not include any scaling by device scale factor, so are logical pixels
  // not physical device pixels.
  ScreenInfo screen_info_;
  // The screen rects of the view and the window that contains it. These do not
  // include any scaling by device scale factor, so are logical pixels not
  // physical device pixels.
  gfx::Rect widget_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager_;

  std::unique_ptr<RenderWidgetInputHandler> input_handler_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // True if the IME requests updated composition info.
  bool monitor_composition_info_ = false;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;

  // Lists of RenderFrameProxy objects for which this RenderWidget is their
  // local root. Each of these represents a child local root RenderWidget in
  // another RenderView frame tree. For values that are propagated from
  // a parent RenderWidget to its children, they are plumbed through the
  // RenderFrameProxys in this list, which bounce those values through the
  // browser to the child RenderWidget in the correct process.
  base::ObserverList<RenderFrameProxy>::Unchecked render_frame_proxies_;

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl>::Unchecked render_frames_;

  base::ObserverList<BrowserPlugin>::Unchecked browser_plugins_;

  bool has_host_context_menu_location_ = false;
  gfx::Point host_context_menu_location_;

  std::unique_ptr<blink::scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;

  // Mouse Lock dispatcher attached to this view.
  std::unique_ptr<RenderWidgetMouseLockDispatcher> mouse_lock_dispatcher_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  std::unique_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  viz::LocalSurfaceIdAllocation local_surface_id_allocation_from_parent_;

  // Indicates whether this widget has focus.
  bool has_focus_ = false;

  // Whether this widget is for a child local root frame. This excludes widgets
  // that are not for a frame (eg popups) and excludes the widget for the main
  // frame (which is attached to the RenderViewImpl).
  bool for_child_local_root_frame_ = false;
  // RenderWidgets are created for frames, popups and pepper fullscreen. In the
  // former case, the caller frame takes ownership and eventually passes the
  // unique_ptr back in Close(). In the latter cases, the browser process takes
  // ownership via IPC.  These booleans exist to allow us to confirm than an IPC
  // message to kill the render widget is coming for a popup or fullscreen.
  bool popup_ = false;
  bool pepper_fullscreen_ = false;

  // A callback into the creator/opener of this widget, to be executed when
  // WebWidgetClient::Show() occurs.
  ShowCallback show_callback_;

#if defined(OS_MACOSX)
  // Responds to IPCs from TextInputClientMac regarding getting string at given
  // position or range as well as finding character index at a given position.
  std::unique_ptr<TextInputClientObserver> text_input_client_observer_;
#endif

  // Stores edit commands associated to the next key event.
  // Will be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  bool first_update_visual_state_after_hidden_ = false;
  base::TimeTicks was_shown_time_ = base::TimeTicks::Now();

  // Object to record tab switch time into this RenderWidget
  TabSwitchTimeRecorder tab_switch_time_recorder_;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  bool browser_controls_shrink_blink_size_ = false;
  // The height of the browser top controls.
  float top_controls_height_ = 0.f;
  // The height of the browser bottom controls.
  float bottom_controls_height_ = 0.f;

  // The last seen page scale state, which comes from the main frame and is
  // propagated through the RenderWidget tree. This state is passed to any new
  // child RenderWidget.
  float page_scale_factor_from_mainframe_ = 1.f;
  bool is_pinch_gesture_active_from_mainframe_ = false;

  scoped_refptr<MainThreadEventQueue> input_event_queue_;

  mojo::Receiver<mojom::Widget> widget_receiver_;

  gfx::Rect compositor_visible_rect_;

  // Different consumers in the browser process makes different assumptions, so
  // must always send the first IPC regardless of value.
  base::Optional<bool> has_touch_handlers_;

  uint32_t last_capture_sequence_number_ = 0u;

  base::WeakPtrFactory<RenderWidget> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_

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
#include "content/common/widget.mojom.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/screen_info.h"
#include "content/public/common/widget_type.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator_delegate.h"
#include "content/renderer/gpu/layer_tree_view_delegate.h"
#include "content/renderer/input/main_thread_event_queue.h"
#include "content/renderer/input/render_widget_input_handler.h"
#include "content/renderer/input/render_widget_input_handler_delegate.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_mouse_lock_dispatcher.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

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
class WebGestureEvent;
class WebInputMethodController;
class WebLocalFrame;
class WebMouseEvent;
struct WebPoint;
}  // namespace blink

namespace cc {
struct ApplyViewportChangesArgs;
class SwapPromise;
}

namespace gfx {
class ColorSpace;
class Range;
}

namespace ui {
struct DidOverscrollParams;
}

namespace content {
class BrowserPlugin;
class CompositorDependencies;
class ExternalPopupMenu;
class FrameSwapMessageQueue;
class ImeEventGuard;
class LayerTreeView;
class MainThreadEventQueue;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderViewImpl;
class RenderWidgetOwnerDelegate;
class RenderWidgetScreenMetricsEmulator;
class ResizingModeSelector;
class TextInputClientObserver;
class WidgetInputHandlerManager;
struct ContextMenuParams;
struct VisualProperties;

// RenderWidget provides a communication bridge between a WebWidget and
// a RenderWidgetHost, the latter of which lives in a different process.
//
// RenderWidget is used to implement:
// - RenderViewImpl (deprecated)
// - Fullscreen mode (RenderWidgetFullScreen)
// - Popup "menus" (like the color chooser and date picker)
// - Widgets for frames (for out-of-process iframe support)
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      public blink::WebWidgetClient,
      public mojom::Widget,
      public LayerTreeViewDelegate,
      public RenderWidgetInputHandlerDelegate,
      public RenderWidgetScreenMetricsEmulatorDelegate,
      public base::RefCounted<RenderWidget>,
      public MainThreadEventQueueClient {
 public:
  using ShowCallback =
      base::OnceCallback<void(RenderWidget* widget_to_show,
                              blink::WebNavigationPolicy policy,
                              const gfx::Rect& initial_rect)>;

  // Time-To-First-Active-Paint(TTFAP) type
  enum {
    TTFAP_AFTER_PURGED,
    TTFAP_5MIN_AFTER_BACKGROUNDED,
  };

  // An Init*() method must be called after creating a RenderWidget, which will
  // make the RenderWidget self-referencing. Then it can be deleted by calling
  // Close().
  RenderWidget(int32_t widget_routing_id,
               CompositorDependencies* compositor_deps,
               WidgetType widget_type,
               const ScreenInfo& screen_info,
               blink::WebDisplayMode display_mode,
               bool swapped_out,
               bool hidden,
               bool never_visible,
               mojom::WidgetRequest widget_request = nullptr);

  // Initialize a new RenderWidget for a popup. The |show_callback| is called
  // when RenderWidget::Show() happens. This method increments the reference
  // count on the RenderWidget, making it self-referencing, which can be
  // released by calling Close().
  void InitForPopup(ShowCallback show_callback,
                    blink::WebPagePopup* web_page_popup);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a local root, but not the main
  // frame. This method increments the reference count on the RenderWidget,
  // making it self-referencing, which can be released by calling Close().
  void InitForChildLocalRoot(blink::WebFrameWidget* web_frame_widget);

  void set_owner_delegate(RenderWidgetOwnerDelegate* owner_delegate) {
    DCHECK(!owner_delegate_);
    owner_delegate_ = owner_delegate;
  }
  RenderWidgetOwnerDelegate* owner_delegate() const { return owner_delegate_; }

  // Returns the RenderWidget for the given routing ID.
  static RenderWidget* FromRoutingID(int32_t routing_id);

  // Closes a RenderWidget that was created by |CreateForFrame|.
  void CloseForFrame();

  int32_t routing_id() const { return routing_id_; }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }

  // This can return nullptr while the RenderWidget is closing.
  blink::WebWidget* GetWebWidget() const;

  // Returns the current instance of WebInputMethodController which is to be
  // used for IME related tasks. This instance corresponds to the one from
  // focused frame and can be nullptr.
  blink::WebInputMethodController* GetInputMethodController() const;

  const gfx::Size& size() const { return size_; }
  const gfx::Size& compositor_viewport_pixel_size() const {
    return compositor_viewport_pixel_size_;
  }
  bool is_fullscreen_granted() const { return is_fullscreen_granted_; }
  blink::WebDisplayMode display_mode() const { return display_mode_; }
  bool is_hidden() const { return is_hidden_; }
  // Temporary for debugging purposes...
  bool closing() const { return closing_; }
  bool has_host_context_menu_location() const {
    return has_host_context_menu_location_;
  }
  gfx::Point host_context_menu_location() const {
    return host_context_menu_location_;
  }
  const gfx::Size& visible_viewport_size() const {
    return visible_viewport_size_;
  }

  ScreenInfo screen_info() const { return screen_info_; }
  void set_screen_info(const ScreenInfo& info) { screen_info_ = info; }

  // Sets whether this RenderWidget has been swapped out to be displayed by
  // a RenderWidget in a different process.  If so, no new IPC messages will be
  // sent (only ACKs) and the process is free to exit when there are no other
  // active RenderWidgets.
  void SetSwappedOut(bool is_swapped_out);
  bool is_swapped_out() const { return is_swapped_out_; }

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
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override;
  void BeginMainFrame(base::TimeTicks frame_time) override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  void DidCommitAndDrawCompositorFrame() override;
  void DidCommitCompositorFrame() override;
  void DidCompletePageScaleAnimation() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time) override;
  bool IsClosing() const override;
  void RequestScheduleAnimation() override;
  void UpdateVisualState() override;
  void WillBeginCompositorFrame() override;
  std::unique_ptr<cc::SwapPromise> RequestCopyOfOutputForLayoutTest(
      std::unique_ptr<viz::CopyOutputRequest> request) override;

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
  void Redraw() override;
  void SynchronizeVisualProperties(
      const VisualProperties& resize_params) override;
  void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) override;
  void SetScreenRects(const gfx::Rect& widget_screen_rect,
                      const gfx::Rect& window_screen_rect) override;

  // blink::WebWidgetClient
  void IntrinsicSizingInfoChanged(
      const blink::WebIntrinsicSizingInfo&) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void DidChangeCursor(const blink::WebCursorInfo&) override;
  void AutoscrollStart(const blink::WebFloatPoint& point) override;
  void AutoscrollFling(const blink::WebFloatSize& velocity) override;
  void AutoscrollEnd() override;
  void CloseWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  blink::WebRect WindowRect() override;
  blink::WebRect ViewRect() override;
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection hint) override;
  void SetWindowRect(const blink::WebRect&) override;
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void DidOverscroll(const blink::WebFloatSize& overscrollDelta,
                     const blink::WebFloatSize& accumulatedOverscroll,
                     const blink::WebFloatPoint& position,
                     const blink::WebFloatSize& velocity,
                     const cc::OverscrollBehavior& behavior) override;
  void ShowVirtualKeyboardOnElementFocus() override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  bool RequestPointerLock() override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void StartDragging(network::mojom::ReferrerPolicy policy,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const SkBitmap& drag_image,
                     const blink::WebPoint& image_offset) override;
  void SetTouchAction(cc::TouchAction touch_action) override;
  void RequestUnbufferedInputEvents() override;
  void HasPointerRawMoveEventHandlers(bool has_handlers) override;
  void HasTouchEventHandlers(bool has_handlers) override;
  void SetNeedsLowLatencyInput(bool) override;

  // Override point to obtain that the current input method state and caret
  // position.
  ui::TextInputType GetTextInputType();

  // Internal helper that generates the LayerTreeSettings to be given to the
  // compositor in StartCompositor().
  static cc::LayerTreeSettings GenerateLayerTreeSettings(
      CompositorDependencies* compositor_deps,
      bool is_for_subframe,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);
  // Internal helper that generates the ManagedMemoryPolicy to be given to the
  // compositor in StartCompositor().
  static cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
      const cc::ManagedMemoryPolicy& policy,
      const gfx::Size& initial_screen_size,
      float initial_device_scale_factor);

  // Begins the compositor's scheduler to start producing frames.
  void StartCompositor();

  // Stop compositing.
  void WillCloseLayerTreeView();

  LayerTreeView* layer_tree_view() const { return layer_tree_view_.get(); }
  WidgetInputHandlerManager* widget_input_handler_manager() {
    return widget_input_handler_manager_.get();
  }
  const RenderWidgetInputHandler& input_handler() const {
    return *input_handler_;
  }

  void SetHandlingInputEvent(bool handling_input_event);

  // Delivers |message| together with compositor state change updates.
  // This mechanism is not a drop-in replacement for IPC: messages sent this way
  // will not be automatically available to BrowserMessageFilter, for example.
  // FIFO ordering is preserved between messages enqueued.
  //
  // |msg| message to send, ownership of |msg| is transferred.
  void QueueMessage(IPC::Message* msg);

  // Handle start and finish of IME event guard.
  void OnImeEventGuardStart(ImeEventGuard* guard);
  void OnImeEventGuardFinish(ImeEventGuard* guard);

  void ApplyEmulatedScreenMetricsForPopupWidget(RenderWidget* origin_widget);

  gfx::Rect AdjustValidationMessageAnchor(const gfx::Rect& anchor);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end);

  void OnShowHostContextMenu(ContextMenuParams* params);

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

  // Returns the ScreenInfo exposed to Blink. In device emulation, this
  // may not match the compositor ScreenInfo.
  const ScreenInfo& GetWebScreenInfo() const;

  // When emulated, this returns the original (non-emulated) ScreenInfo.
  const ScreenInfo& GetOriginalScreenInfo() const;

  // Helper to convert |point| using ConvertWindowToViewport().
  gfx::PointF ConvertWindowPointToViewport(const gfx::PointF& point);
  gfx::Point ConvertWindowPointToViewport(const gfx::Point& point);
  uint32_t GetContentSourceId();
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

  // MainThreadEventQueueClient overrides.

  // Requests a BeginMainFrame callback from the compositor.
  void SetNeedsMainFrame() override;

  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  void HandleInputEvent(const blink::WebCoalescedInputEvent& input_event,
                        const ui::LatencyInfo& latency_info,
                        HandledEventCallback callback) override;

  void SetupWidgetInputHandler(mojom::WidgetInputHandlerRequest request,
                               mojom::WidgetInputHandlerHostPtr host) override;

  scoped_refptr<MainThreadEventQueue> GetInputEventQueue();

  void OnSetActive(bool active);
  void OnSetFocus(bool enable);
  void OnSetBackgroundOpaque(bool opaque);
  void OnMouseCaptureLost();
  void OnCursorVisibilityChange(bool is_visible);
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

  // This does the actual focus change, but is called in more situations than
  // just as an IPC message.
  void SetFocus(bool enable);

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
  void SetWidgetBinding(mojom::WidgetRequest request);

  void SetMouseCapture(bool capture);

  void SetWindowScreenRect(const gfx::Rect& window_screen_rect);

  bool IsSurfaceSynchronizationEnabled() const;

  void UseSynchronousResizeModeForTesting(bool enable);
  void SetDeviceScaleFactorForTesting(float factor);
  void SetDeviceColorSpaceForTesting(const gfx::ColorSpace& color_space);
  void SetWindowRectSynchronouslyForTesting(const gfx::Rect& new_window_rect);
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  base::WeakPtr<RenderWidget> AsWeakPtr();

 protected:
  ~RenderWidget() override;

  // Called by Create() functions and subclasses to finish initialization.
  // |show_callback| will be invoked once WebWidgetClient::Show() occurs, and
  // should be null if Show() won't be triggered for this widget.
  void Init(ShowCallback show_callback, blink::WebWidget* web_widget);

  // Update the web view's device scale factor.
  void UpdateWebViewWithDeviceScaleFactor();

  // Close the underlying WebWidget and stop the compositor.
  virtual void Close();

  // Notify subclasses that we initiated the paint operation.
  virtual void DidInitiatePaint() {}

  // RenderWidget IPC message handler that can be overridden by subclasses.
  virtual void OnSynchronizeVisualProperties(const VisualProperties& params);

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
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetPopupUnittest, EmulatingPopupRect);

  static scoped_refptr<base::SingleThreadTaskRunner> GetCleanupTaskRunner();

  LayerTreeView* InitializeLayerTreeView();

  void DoDeferredClose();
  void NotifyOnClose();

  gfx::Size GetSizeForWebWidget() const;
  void ResizeWebWidget();

  // Just Close the WebWidget, in cases where the Close() will be deferred.
  // It is safe to call this multiple times, which happens in the case of
  // frame widgets beings closed, since subsequent calls are ignored.
  void CloseWebWidget();

#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void SetExternalPopupOriginAdjustmentsForEmulation(
      ExternalPopupMenu* popup,
      RenderWidgetScreenMetricsEmulator* emulator);
#endif

  // RenderWidget IPC message handlers.
  void OnHandleInputEvent(
      const blink::WebInputEvent* event,
      const std::vector<const blink::WebInputEvent*>& coalesced_events,
      const ui::LatencyInfo& latency_info,
      InputEventDispatchType dispatch_type);
  void OnClose();
  void OnCreatingNewAck();
  void OnEnableDeviceEmulation(const blink::WebDeviceEmulationParams& params);
  void OnDisableDeviceEmulation();
  void OnWasHidden();
  void OnWasShown(base::TimeTicks show_request_timestamp);
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
  void OnSetViewportIntersection(const gfx::Rect& viewport_intersection,
                                 const gfx::Rect& compositor_visible_rect,
                                 bool occluded_or_obscured);
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

  // Sets the "hidden" state of this widget.  All accesses to is_hidden_ should
  // use this method so that we can properly inform the RenderThread of our
  // state.
  void SetHidden(bool hidden);

  void DidToggleFullscreen();

  // Returns a rect that the compositor needs to raster. For a main frame this
  // is always the entire viewport, but for out-of-process iframes this can be
  // constrained to limit overdraw.
  gfx::Rect ViewportVisibleRect();

  // QueueMessage implementation extracted into a static method for easy
  // testing.
  static std::unique_ptr<cc::SwapPromise> QueueMessageImpl(
      IPC::Message* msg,
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

  // TODO(ekaramad): This method should not be confused with its RenderView
  // variant, GetWebFrameWidget(). Currently Cast and AndroidWebview's
  // ContentRendererClients are the only users of the public variant. The public
  // method will eventually be removed from RenderView and uses of the method
  // will obtain WebFrameWidget from WebLocalFrame.
  // Returns the WebFrameWidget associated with this RenderWidget if any.
  // Returns nullptr if GetWebWidget() returns nullptr or returns a WebWidget
  // that is not a WebFrameWidget. A WebFrameWidget only makes sense when there
  // a local root associated with it. RenderWidgetFullscreenPepper and a swapped
  // out RenderWidgets are amongst the cases where this method returns nullptr.
  blink::WebFrameWidget* GetFrameWidget() const;

  // Applies/Removes the DevTools device emulation transformation to/from a
  // window rect.
  void ScreenRectToEmulatedIfNeeded(blink::WebRect* window_rect) const;
  void EmulatedToScreenRectIfNeeded(blink::WebRect* window_rect) const;

  void UpdateSurfaceAndScreenInfo(
      const viz::LocalSurfaceId& new_local_surface_id,
      base::TimeTicks new_allocation_time_,
      const gfx::Size& new_compositor_viewport_pixel_size,
      const ScreenInfo& new_screen_info);

  // Used to force the size of a window when running layout tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);

  void UpdateCaptureSequenceNumber(uint32_t capture_sequence_number);

  // A variant of Send but is fatal if it fails. The browser may
  // be waiting for this IPC Message and if the send fails the browser will
  // be left in a state waiting for something that never comes. And if it
  // never comes then it may later determine this is a hung renderer; so
  // instead fail right away.
  void SendOrCrash(IPC::Message* msg);

  // Determines whether or not RenderWidget should process IME events from the
  // browser. It always returns true unless there is no WebFrameWidget to
  // handle the event, or there is no page focus.
  bool ShouldHandleImeEvents() const;

  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool reply_to_request);

  gfx::ColorSpace GetRasterColorSpace() const;

  void SendInputEventAck(blink::WebInputEvent::Type type,
                         uint32_t touch_event_id,
                         InputEventAckState ack_state,
                         const ui::LatencyInfo& latency_info,
                         std::unique_ptr<ui::DidOverscrollParams>,
                         base::Optional<cc::TouchAction>);

  void UpdateZoom(double zoom_level);

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

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost.
  const int32_t routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // Use GetWebWidget() instead of using webwidget_internal_ directly.
  // We are responsible for destroying this object via its Close method.
  // May be NULL when the window is closing.
  blink::WebWidget* webwidget_internal_;

  // The delegate of the owner of this object.
  RenderWidgetOwnerDelegate* owner_delegate_;

  // This is lazily constructed and must not outlive webwidget_.
  std::unique_ptr<LayerTreeView> layer_tree_view_;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // We store the current cursor object so we can avoid spamming SetCursor
  // messages.
  WebCursor current_cursor_;

  base::Optional<float> device_scale_factor_for_testing_;

  // The size of the RenderWidget in DIPs. This may differ from
  // |compositor_viewport_pixel_size_| in the following (and possibly other)
  // cases: * On Android, for top and bottom controls * On OOPIF, due to
  // rounding
  gfx::Size size_;

  // The size of the compositor's surface in pixels.
  gfx::Size compositor_viewport_pixel_size_;

  // The size of the visible viewport in pixels.
  gfx::Size visible_viewport_size_;

  // Whether the WebWidget is in auto resize mode, which is used for example
  // by extension popups.
  bool auto_resize_mode_;

  // The minimum size to use for auto-resize.
  gfx::Size min_size_for_auto_resize_;

  // The maximum size to use for auto-resize.
  gfx::Size max_size_for_auto_resize_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool compositor_never_visible_;

  // Indicates whether tab-initiated fullscreen was granted.
  bool is_fullscreen_granted_;

  // Indicates the display mode.
  blink::WebDisplayMode display_mode_;

  // It is possible that one ImeEventGuard is nested inside another
  // ImeEventGuard. We keep track of the outermost one, and update it as needed.
  ImeEventGuard* ime_event_guard_;

  // True if we have requested this widget be closed.  No more messages will
  // be sent, except for a Close.
  bool closing_;

  // True if it is known that the host is in the process of being shut down.
  bool host_closing_;

  // Whether this RenderWidget is currently swapped out, such that the view is
  // being rendered by another process.  If all RenderWidgets in a process are
  // swapped out, the process can exit.
  bool is_swapped_out_;

  // Stores information about the current text input.
  blink::WebTextInputInfo text_input_info_;

  // Stores the current text input type of |webwidget_|.
  ui::TextInputType text_input_type_;

  // Stores the current text input mode of |webwidget_|.
  ui::TextInputMode text_input_mode_;

  // Stores the current text input flags of |webwidget_|.
  int text_input_flags_;

  // Indicates whether currently focused input field has next/previous focusable
  // form input field.
  int next_previous_flags_;

  // Stores the current type of composition text rendering of |webwidget_|.
  bool can_compose_inline_;

  // Stores the current selection bounds.
  gfx::Rect selection_focus_rect_;
  gfx::Rect selection_anchor_rect_;

  // Stores the current composition character bounds.
  std::vector<gfx::Rect> composition_character_bounds_;

  // Stores the current composition range.
  gfx::Range composition_range_;

  // The kind of popup this widget represents, NONE if not a popup.
  WidgetType widget_type_;

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_;
  gfx::Rect pending_window_rect_;

  // The screen rects of the view and the window that contains it.
  gfx::Rect widget_screen_rect_;
  gfx::Rect window_screen_rect_;

  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager_;

  std::unique_ptr<RenderWidgetInputHandler> input_handler_;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

  // Properties of the screen hosting this RenderWidget instance.
  ScreenInfo screen_info_;

  // True if the IME requests updated composition info.
  bool monitor_composition_info_;

  std::unique_ptr<RenderWidgetScreenMetricsEmulator> screen_metrics_emulator_;

  // Popups may be displaced when screen metrics emulation is enabled.
  // These values are used to properly adjust popup position.
  gfx::Point popup_view_origin_for_emulation_;
  gfx::Point popup_screen_origin_for_emulation_;
  float popup_origin_scale_for_emulation_;

  scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue_;
  std::unique_ptr<ResizingModeSelector> resizing_mode_selector_;

  // Lists of RenderFrameProxy objects that need to be notified of
  // compositing-related events (e.g. DidCommitCompositorFrame).
  base::ObserverList<RenderFrameProxy>::Unchecked render_frame_proxies_;

  // A list of RenderFrames associated with this RenderWidget. Notifications
  // are sent to each frame in the list for events such as changing
  // visibility state for example.
  base::ObserverList<RenderFrameImpl>::Unchecked render_frames_;

  base::ObserverList<BrowserPlugin>::Unchecked browser_plugins_;

  bool has_host_context_menu_location_;
  gfx::Point host_context_menu_location_;

  std::unique_ptr<blink::scheduler::WebRenderWidgetSchedulingState>
      render_widget_scheduling_state_;

  // Mouse Lock dispatcher attached to this view.
  std::unique_ptr<RenderWidgetMouseLockDispatcher> mouse_lock_dispatcher_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  std::unique_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  viz::LocalSurfaceId local_surface_id_from_parent_;
  base::TimeTicks allocation_time_from_parent_;

  // Indicates whether this widget has focus.
  bool has_focus_;

  // Whether this RenderWidget is for an out-of-process iframe or not.
  bool for_oopif_;

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

  bool first_update_visual_state_after_hidden_;
  base::TimeTicks was_shown_time_;

  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar.
  bool browser_controls_shrink_blink_size_ = false;
  // The height of the browser top controls.
  float top_controls_height_ = 0.f;
  // The height of the browser bottom controls.
  float bottom_controls_height_ = 0.f;

  // This is initialized to zero and is incremented on each non-same-page
  // navigation commit by RenderFrameImpl. At that time it is sent to the
  // compositor so that it can tag compositor frames, and RenderFrameImpl is
  // responsible for sending it to the browser process to be used to match
  // each compositor frame to the most recent page navigation before it was
  // generated.
  // This only applies to main frames, and is not touched for subframe
  // RenderWidgets, where there is no concern around displaying unloaded
  // content.
  // TODO(kenrb, fsamuel): This should be removed when SurfaceIDs can be used
  // to replace it. See https://crbug.com/695579.
  uint32_t current_content_source_id_;

  scoped_refptr<MainThreadEventQueue> input_event_queue_;

  mojo::Binding<mojom::Widget> widget_binding_;

  gfx::Rect compositor_visible_rect_;

  // Different consumers in the browser process makes different assumptions, so
  // must always send the first IPC regardless of value.
  base::Optional<bool> has_touch_handlers_;

  uint32_t last_capture_sequence_number_ = 0u;

  base::WeakPtrFactory<RenderWidget> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_

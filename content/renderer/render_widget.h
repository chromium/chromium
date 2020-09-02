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
#include "cc/trees/browser_controls_params.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/managed_memory_policy.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/content_to_visible_time_reporter.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/renderer.mojom-forward.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_delegate.h"
#include "content/renderer/render_widget_mouse_lock_dispatcher.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/widget/screen_info.h"
#include "third_party/blink/public/mojom/page/record_content_to_visible_time_request.mojom-forward.h"
#include "third_party/blink/public/platform/viewport_intersection_state.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_text_input_info.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"
#include "ui/surface/transport_dib.h"

namespace blink {
struct VisualProperties;
class WebDragData;
class WebFrameWidget;
class WebInputMethodController;
class WebLocalFrame;
class WebMouseEvent;
class WebPagePopup;
}  // namespace blink

namespace gfx {
class ColorSpace;
struct PresentationFeedback;
class Range;
}

namespace content {
class CompositorDependencies;
class PepperPluginInstanceImpl;
class RenderFrameImpl;
class RenderFrameProxy;
class RenderViewImpl;
class RenderWidgetDelegate;

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
class CONTENT_EXPORT RenderWidget
    : public IPC::Listener,
      public IPC::Sender,
      public blink::WebPagePopupClient {  // Is-a WebWidgetClient also
 public:
  RenderWidget(int32_t widget_routing_id,
               CompositorDependencies* compositor_deps,
               bool hidden,
               bool never_composited);

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
  using CreateRenderWidgetFunction =
      std::unique_ptr<RenderWidget> (*)(int32_t routing_id,
                                        CompositorDependencies*,
                                        bool hidden,
                                        bool never_composited);
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
      bool never_composited);

  // Creates a RenderWidget for a popup. This is separate from CreateForFrame()
  // because popups do not not need to be faked out.
  // A RenderWidget popup is owned by the browser process. The object will be
  // destroyed by the WidgetMsg_Close message. The object can request its own
  // destruction via ClosePopupWidgetSoon().
  static RenderWidget* CreateForPopup(int32_t widget_routing_id,
                                      CompositorDependencies* compositor_deps,
                                      bool hidden,
                                      bool never_composited);

  // Initialize a new RenderWidget for a popup. The |show_callback| is called
  // when RenderWidget::Show() happens. The |opener_widget| is the local root
  // of the frame that is opening the popup.
  void InitForPopup(ShowCallback show_callback,
                    RenderWidget* opener_widget,
                    blink::WebPagePopup* web_page_popup,
                    const blink::ScreenInfo& screen_info);

  // Initialize a new RenderWidget for pepper fullscreen. The |show_callback| is
  // called when RenderWidget::Show() happens.
  void InitForPepperFullscreen(ShowCallback show_callback,
                               blink::WebWidget* web_widget,
                               const blink::ScreenInfo& screen_info);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a main frame.
  void InitForMainFrame(ShowCallback show_callback,
                        blink::WebFrameWidget* web_frame_widget,
                        const blink::ScreenInfo& screen_info,
                        RenderWidgetDelegate& delegate);

  // Initialize a new RenderWidget that will be attached to a RenderFrame (via
  // the WebFrameWidget), for a frame that is a local root, but not the main
  // frame.
  void InitForChildLocalRoot(blink::WebFrameWidget* web_frame_widget,
                             const blink::ScreenInfo& screen_info);

  RenderWidgetDelegate* delegate() const { return delegate_; }

  // Closes a RenderWidget that was created by |CreateForFrame|. Ownership is
  // passed into this object to asynchronously delete itself.
  void CloseForFrame(std::unique_ptr<RenderWidget> widget);

  int32_t routing_id() const { return routing_id_; }

  CompositorDependencies* compositor_deps() const { return compositor_deps_; }

  // This can return nullptr while the RenderWidget is closing. When for_frame()
  // is true, the widget returned is a blink::WebFrameWidget.
  blink::WebWidget* GetWebWidget() const { return webwidget_; }

  // Returns the current instance of WebInputMethodController which is to be
  // used for IME related tasks. This instance corresponds to the one from
  // focused frame and can be nullptr.
  blink::WebInputMethodController* GetInputMethodController() const;

  const gfx::Size& size() const { return size_; }
  bool is_hidden() const { return is_hidden_; }

  // A main frame RenderWidget is destroyed and recreated using the same routing
  // id. So messages en route to a destroyed RenderWidget may end up being
  // received by a provisional RenderWidget, even though we don't normally
  // communicate with a RenderWidget for a provisional frame. This can be used
  // to avoid that race condition of acting on IPC messages meant for a
  // destroyed RenderWidget.
  bool IsForProvisionalFrame() const;

  // Functions to track out-of-process frames for special notifications.
  void RegisterRenderFrameProxy(RenderFrameProxy* proxy);
  void UnregisterRenderFrameProxy(RenderFrameProxy* proxy);

  // Functions to track all RenderFrame objects associated with this
  // RenderWidget.
  void RegisterRenderFrame(RenderFrameImpl* frame);
  void UnregisterRenderFrame(RenderFrameImpl* frame);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // IPC::Sender
  bool Send(IPC::Message* msg) override;

  // blink::WebWidgetClient
  void ScheduleAnimation() override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void CloseWidgetSoon() override;
  void ClosePopupWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  void SetWindowRect(const gfx::Rect&) override;
  void SetSize(const gfx::Size&) override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertViewportToWindow(blink::WebFloatRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  gfx::Point ConvertWindowPointToViewport(const gfx::Point& point) override;
  gfx::PointF ConvertWindowPointToViewport(const gfx::PointF& point) override;
  bool RequestPointerLock(blink::WebLocalFrame* requester_frame,
                          blink::WebWidgetClient::PointerLockCallback callback,
                          bool request_unadjusted_movement) override;
  bool RequestPointerLockChange(
      blink::WebLocalFrame* requester_frame,
      blink::WebWidgetClient::PointerLockCallback callback,
      bool request_unadjusted_movement) override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void StartDragging(const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const SkBitmap& drag_image,
                     const gfx::Point& image_offset) override;
  void RequestDecode(const cc::PaintImage& image,
                     base::OnceCallback<void(bool)> callback) override;
  viz::FrameSinkId GetFrameSinkId() override;
  void RecordTimeToFirstActivePaint(base::TimeDelta duration) override;
  void DidCommitAndDrawCompositorFrame() override;
  void DidCommitCompositorFrame(base::TimeTicks commit_start_time) override;
  void DidCompletePageScaleAnimation() override;
  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override;
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override;
  bool WillHandleMouseEvent(const blink::WebMouseEvent& event) override;
  bool CanComposeInline() override;
  bool ShouldDispatchImeEventsToPepper() override;
  blink::WebTextInputType GetPepperTextInputType() override;
  gfx::Rect GetPepperCaretBounds() override;
  void FocusChanged(bool enable) override;
  void ImeSetCompositionForPepper(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int selection_start,
      int selection_end) override;
  void ImeCommitTextForPepper(
      const blink::WebString& text,
      const std::vector<ui::ImeTextSpan>& ime_text_spans,
      const gfx::Range& replacement_range,
      int relative_cursor_pos) override;
  void ImeFinishComposingTextForPepper(bool keep_selection) override;

  void UpdateTextInputState();

  cc::LayerTreeHost* layer_tree_host() { return layer_tree_host_; }
  void SetHandlingInputEvent(bool handling_input_event);

  // Checks if the selection bounds have been changed. If they are changed,
  // the new value will be sent to the browser process.
  void UpdateSelectionBounds();

  // Called when the Widget has changed size as a result of an auto-resize.
  void DidAutoResize(const gfx::Size& new_size);

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

  void DidNavigate(ukm::SourceId source_id, const GURL& url);

  viz::FrameSinkId GetFrameSinkIdAtPoint(const gfx::PointF& point,
                                         gfx::PointF* local_point);

  void SetActive(bool active);

  void UseSynchronousResizeModeForTesting(bool enable);
  void SetDeviceScaleFactorForTesting(float factor);
  void SetWindowRectSynchronouslyForTesting(const gfx::Rect& new_window_rect);

  // Do a hit test for a given point in viewport coordinate.
  blink::WebHitTestResult GetHitTestResultAtPoint(const gfx::PointF& point);

  // Forces a redraw and invokes the callback once the frame's been displayed
  // to the user in the display compositor.
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void RequestPresentation(PresentationTimeCallback callback);

  // Determines if fullscreen is granted for the frame.
  bool IsFullscreenGrantedForFrame();

 protected:
  // blink::WebWidgetClient
  void UpdateVisualProperties(
      bool emulator_enabled,
      const blink::VisualProperties& properties) override;

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
  friend class RenderWidgetTest;
  friend class RenderViewImplTest;

  void Initialize(ShowCallback show_callback,
                  blink::WebWidget* web_widget,
                  const blink::ScreenInfo& screen_info);
  // Initializes the compositor and dependent systems, as part of the
  // Initialize() process.
  void InitCompositing(const blink::ScreenInfo& screen_info);

  // Request the window to close from the renderer by sending the request to the
  // browser.
  static void DoDeferredClose(int widget_routing_id);

  // Must be called to pass updated values to blink when the widget size, the
  // visual viewport size, or the device scale factor change.
  void ResizeWebWidget();

  // RenderWidget IPC message handlers.
  void OnClose();
  void OnCreatingNewAck();
  void OnWasHidden();
  void OnWasShown(base::TimeTicks show_request_timestamp,
                  bool was_evicted,
                  const blink::mojom::RecordContentToVisibleTimeRequestPtr&
                      record_tab_switch_time_request);
  void OnRequestSetBoundsAck();

  void OnSetViewportIntersection(
      const blink::ViewportIntersectionState& intersection_state);
  void OnDragTargetDragEnter(
      const std::vector<DropData::Metadata>& drop_meta_data,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers);
  void OnDragSourceEnded(const gfx::PointF& client_point,
                         const gfx::PointF& screen_point,
                         blink::WebDragOperation drag_operation);

  // Sets the "hidden" state of this widget.  All modification of is_hidden_
  // should use this method so that we can properly inform the RenderThread of
  // our state.
  void SetHidden(bool hidden);

  // Set the pending window rect.
  // Because the real render_widget is hosted in another process, there is
  // a time period where we may have set a new window rect which has not yet
  // been processed by the browser.  So we maintain a pending window rect
  // size.  If JS code sets the WindowRect, and then immediately calls
  // GetWindowRect() we'll use this pending window rect as the size.
  void SetPendingWindowRect(const gfx::Rect& r);

  // Returns the WebFrameWidget associated with this RenderWidget if any.
  // Returns nullptr if GetWebWidget() returns nullptr or returns a WebWidget
  // that is not a WebFrameWidget. A WebFrameWidget only makes sense when there
  // a local root associated with it. RenderWidgetFullscreenPepper and a swapped
  // out RenderWidgets are amongst the cases where this method returns nullptr.
  blink::WebFrameWidget* GetFrameWidget() const;

  // Used to force the size of a window when running web tests.
  void SetWindowRectSynchronously(const gfx::Rect& new_window_rect);

  void UpdateTextInputStateInternal(bool show_virtual_keyboard,
                                    bool reply_to_request);

  gfx::ColorSpace GetRasterColorSpace() const;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Returns the focused pepper plugin, if any, inside the WebWidget. That is
  // the pepper plugin which is focused inside a frame which belongs to the
  // local root associated with this RenderWidget.
  PepperPluginInstanceImpl* GetFocusedPepperPluginInsideWidget();
#endif

  bool AutoResizeMode();

  // Whether this widget is for a frame. This excludes widgets that are not for
  // a frame (eg popups, pepper), but includes both the main frame
  // (via delegate_) and subframes (via for_child_local_root_frame_).
  bool for_frame() const { return delegate_ || for_child_local_root_frame_; }

  // Routing ID that allows us to communicate to the parent browser process
  // RenderWidgetHost.
  const int32_t routing_id_;

  // Dependencies for initializing a compositor, including flags for optional
  // features.
  CompositorDependencies* const compositor_deps_;

  // The delegate for this object which is just a RenderViewImpl.
  // This member is non-null if and only if the RenderWidget is associated with
  // a RenderViewImpl.
  RenderWidgetDelegate* delegate_ = nullptr;

  // We are responsible for destroying this object via its Close method, unless
  // the RenderWidget is associated with a RenderViewImpl through |delegate_|.
  // Becomes null once close is initiated on the RenderWidget.
  blink::WebWidget* webwidget_ = nullptr;

  // This is valid while |webwidget_| is valid.
  cc::LayerTreeHost* layer_tree_host_ = nullptr;

  // The rect where this view should be initially shown.
  gfx::Rect initial_rect_;

  // The size of the RenderWidget in DIPs. This may differ from the viewport
  // set in the compositor, as the viewport can be a subset of the RenderWidget
  // in such cases as:
  // - When (hiding-on-scroll) top and bottom controls are present.
  // - Rounding issues with OOPIFs (??).
  gfx::Size size_;

  // Indicates that we shouldn't bother generated paint events.
  bool is_hidden_;

  // Indicates that we are never visible, so never produce graphical output.
  const bool never_composited_;

  // True once Close() is called, during the self-destruction process, and to
  // verify destruction always goes through Close().
  bool closing_ = false;

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

  // While we are waiting for the browser to update window sizes, we track the
  // pending size temporarily.
  int pending_window_rect_count_ = 0;

  // The time spent in input handlers this frame. Used to throttle input acks.
  base::TimeDelta total_input_handling_time_this_frame_;

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

  // Mouse Lock dispatcher attached to this view.
  std::unique_ptr<RenderWidgetMouseLockDispatcher> mouse_lock_dispatcher_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  std::unique_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  // Whether this widget is for a child local root frame. This excludes widgets
  // that are not for a frame (eg popups) and excludes the widget for the main
  // frame (which is attached to the RenderViewImpl).
  bool for_child_local_root_frame_ = false;
  // RenderWidgets are created for frames, popups and pepper fullscreen. In the
  // former case, the caller frame takes ownership and eventually passes the
  // unique_ptr back in Close(). In the latter cases, the browser process takes
  // ownership via IPC.  These booleans exist to allow us to confirm than an IPC
  // message to kill the render widget is coming for a popup or fullscreen.
  bool for_popup_ = false;
  bool for_pepper_fullscreen_ = false;

  // A callback into the creator/opener of this widget, to be executed when
  // WebWidgetClient::Show() occurs.
  ShowCallback show_callback_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  // Object to record tab switch time into this RenderWidget
  ContentToVisibleTimeReporter tab_switch_time_recorder_;

  // Browser controls params such as top and bottom controls heights, whether
  // controls shrink blink size etc.
  cc::BrowserControlsParams browser_controls_params_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidget);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/renderer_host/event_with_latency_info.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/input/input_disposition_handler.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/render_widget_host_latency_tracker.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_controller.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/input/input_handler.mojom.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/render_widget_surface_properties.h"
#include "content/common/widget.mojom.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/url_constants.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/hit_test/input_target_client.mojom.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/latency/latency_info.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

#if defined(OS_MACOSX)
#include "services/device/public/mojom/wake_lock.mojom.h"
#endif

class SkBitmap;
struct WidgetHostMsg_SelectionBounds_Params;

namespace blink {
class WebInputEvent;
class WebMouseEvent;
}

namespace cc {
struct BeginFrameAck;
}  // namespace cc

namespace gfx {
class Image;
class Range;
class Vector2dF;
}

namespace ui {
enum class DomCode;
}

namespace viz {
class ServerSharedBitmapManager;
}

namespace content {
class BrowserAccessibilityManager;
class FlingSchedulerBase;
class InputRouter;
class MockRenderWidgetHost;
class PeakGpuMemoryTracker;
class RenderViewHost;
class RenderWidgetHostOwnerDelegate;
class SyntheticGestureController;
class TimeoutMonitor;
class TouchEmulator;
class WebCursor;
struct EditCommand;
struct VisualProperties;
struct ScreenInfo;
struct TextInputState;

// This implements the RenderWidgetHost interface that is exposed to
// embedders of content, and adds things only visible to content.
//
// Several core rendering primitives are mirrored between the browser and
// renderer. These are RenderWidget, RenderFrame and RenderView. Their browser
// counterparts are RenderWidgetHost, RenderFrameHost and RenderViewHost.
//
// For simplicity and clarity, we want the object ownership graph in the
// renderer to mirror the object ownership graph in the browser. The IPC message
// that tears down the renderer object graph should be targeted at the root
// object, and should be sent by the destructor/finalizer of the root object in
// the browser.
//
// Note: We must tear down the renderer object graph with a single IPC to avoid
// inconsistencies in renderer state.
//
// RenderWidget represents a surface that can paint and receive input. It is
// used in four contexts:
//   * Main frame for webpage
//   * Child frame for webpage
//   * Popups
//   * Pepper Fullscreen
//
// In the first two cases, the RenderFrame is not the root of the renderer
// object graph. For the main frame, the root is the RenderView. For child
// frames, the root is RenderFrame. As such, for the first two cases,
// destruction of the RenderWidgetHost will not trigger destruction of the
// RenderWidget.
//
// Note: We want to converge on RenderFrame always being the root.
class CONTENT_EXPORT RenderWidgetHostImpl
    : public RenderWidgetHost,
      public FrameTokenMessageQueue::Client,
      public InputRouterImplClient,
      public InputDispositionHandler,
      public RenderProcessHostImpl::PriorityClient,
      public RenderProcessHostObserver,
      public SyntheticGestureController::Delegate,
      public viz::mojom::CompositorFrameSink,
      public IPC::Listener,
      public RenderFrameMetadataProvider::Observer {
 public:
  // |routing_id| must not be MSG_ROUTING_NONE.
  // If this object outlives |delegate|, DetachDelegate() must be called when
  // |delegate| goes away.
  RenderWidgetHostImpl(
      RenderWidgetHostDelegate* delegate,
      RenderProcessHost* process,
      int32_t routing_id,
      mojo::PendingRemote<mojom::Widget> widget_interface,
      bool hidden,
      std::unique_ptr<FrameTokenMessageQueue> frame_token_message_queue);

  ~RenderWidgetHostImpl() override;

  // Similar to RenderWidgetHost::FromID, but returning the Impl object.
  static RenderWidgetHostImpl* FromID(int32_t process_id, int32_t routing_id);

  // Returns all RenderWidgetHosts including swapped out ones for
  // internal use. The public interface
  // RenderWidgetHost::GetRenderWidgetHosts only returns active ones.
  static std::unique_ptr<RenderWidgetHostIterator> GetAllRenderWidgetHosts();

  // Use RenderWidgetHostImpl::From(rwh) to downcast a RenderWidgetHost to a
  // RenderWidgetHostImpl.
  static RenderWidgetHostImpl* From(RenderWidgetHost* rwh);

  void set_hung_renderer_delay(const base::TimeDelta& delay) {
    hung_renderer_delay_ = delay;
  }

  base::TimeDelta hung_renderer_delay() { return hung_renderer_delay_; }

  void set_new_content_rendering_delay_for_testing(
      const base::TimeDelta& delay) {
    new_content_rendering_delay_ = delay;
  }

  base::TimeDelta new_content_rendering_delay() {
    return new_content_rendering_delay_;
  }

  void set_owner_delegate(RenderWidgetHostOwnerDelegate* owner_delegate) {
    owner_delegate_ = owner_delegate;
  }

  RenderWidgetHostOwnerDelegate* owner_delegate() { return owner_delegate_; }

  void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }


  // RenderWidgetHost implementation.
  const viz::FrameSinkId& GetFrameSinkId() override;
  void UpdateTextDirection(blink::WebTextDirection direction) override;
  void NotifyTextDirection() override;
  void Focus() override;
  void Blur() override;
  void FlushForTesting() override;
  void SetActive(bool active) override;
  void ForwardMouseEvent(const blink::WebMouseEvent& mouse_event) override;
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& wheel_event) override;
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event) override;
  void ForwardGestureEvent(
      const blink::WebGestureEvent& gesture_event) override;
  RenderProcessHost* GetProcess() override;
  int GetRoutingID() override;
  RenderWidgetHostViewBase* GetView() override;
  bool IsCurrentlyUnresponsive() override;
  bool SynchronizeVisualProperties() override;
  void AddKeyPressEventCallback(const KeyPressEventCallback& callback) override;
  void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) override;
  void AddMouseEventCallback(const MouseEventCallback& callback) override;
  void RemoveMouseEventCallback(const MouseEventCallback& callback) override;
  void AddInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
  void RemoveInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
  void AddObserver(RenderWidgetHostObserver* observer) override;
  void RemoveObserver(RenderWidgetHostObserver* observer) override;
  void GetScreenInfo(content::ScreenInfo* result) override;
  // |drop_data| must have been filtered. The embedder should call
  // FilterDropData before passing the drop data to RWHI.
  void DragTargetDragEnter(const DropData& drop_data,
                           const gfx::PointF& client_pt,
                           const gfx::PointF& screen_pt,
                           blink::WebDragOperationsMask operations_allowed,
                           int key_modifiers) override;
  void DragTargetDragEnterWithMetaData(
      const std::vector<DropData::Metadata>& metadata,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) override;
  void DragTargetDragOver(const gfx::PointF& client_pt,
                          const gfx::PointF& screen_pt,
                          blink::WebDragOperationsMask operations_allowed,
                          int key_modifiers) override;
  void DragTargetDragLeave(const gfx::PointF& client_point,
                           const gfx::PointF& screen_point) override;
  // |drop_data| must have been filtered. The embedder should call
  // FilterDropData before passing the drop data to RWHI.
  void DragTargetDrop(const DropData& drop_data,
                      const gfx::PointF& client_pt,
                      const gfx::PointF& screen_pt,
                      int key_modifiers) override;
  void DragSourceEndedAt(const gfx::PointF& client_pt,
                         const gfx::PointF& screen_pt,
                         blink::WebDragOperation operation) override;
  void DragSourceSystemDragEnded() override;
  void FilterDropData(DropData* drop_data) override;
  void SetCursor(const CursorInfo& cursor_info) override;

  // RenderProcessHostImpl::PriorityClient implementation.
  RenderProcessHost::Priority GetPriority() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

  // Notification that the screen info has changed.
  void NotifyScreenInfoChanged();

  // Forces redraw in the renderer and when the update reaches the browser.
  // grabs snapshot from the compositor.
  // If |from_surface| is false, it will obtain the snapshot directly from the
  // view (On MacOS, the snapshot is taken from the Cocoa view for end-to-end
  // testing  purposes).
  // Otherwise, the snapshot is obtained from the view's surface, with no bounds
  // defined.
  // Returns a gfx::Image that is backed by an NSImage on MacOS or by an
  // SkBitmap otherwise. The gfx::Image may be empty if the snapshot failed.
  using GetSnapshotFromBrowserCallback =
      base::OnceCallback<void(const gfx::Image&)>;
  void GetSnapshotFromBrowser(GetSnapshotFromBrowserCallback callback,
                              bool from_surface);

  // Sets the View of this RenderWidgetHost.
  void SetView(RenderWidgetHostViewBase* view);

  RenderWidgetHostDelegate* delegate() const { return delegate_; }

  // Called when a renderer object already been created for this host, and we
  // just need to be attached to it. Used for window.open, <select> dropdown
  // menus, and other times when the renderer initiates creating an object.
  void Init();

  // Initializes a RenderWidgetHost that is attached to a RenderFrameHost.
  void InitForFrame();

  // Returns true if the frame content needs be stored before being evicted.
  bool ShouldShowStaleContentOnEviction();

  // Signal whether this RenderWidgetHost is owned by a RenderFrameHost, in
  // which case it does not do self-deletion.
  void set_owned_by_render_frame_host(bool owned_by_rfh) {
    owned_by_render_frame_host_ = owned_by_rfh;
  }
  bool owned_by_render_frame_host() const {
    return owned_by_render_frame_host_;
  }

  void SetFrameDepth(unsigned int depth);
  void SetIntersectsViewport(bool intersects);
  void UpdatePriority();

  // Tells the renderer to die and optionally delete |this|.
  void ShutdownAndDestroyWidget(bool also_delete);

  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Sends a message to the corresponding object in the renderer.
  bool Send(IPC::Message* msg) override;

  // Indicates if the page has finished loading.
  void SetIsLoading(bool is_loading);

  // Called to notify the RenderWidget that it has been hidden or restored from
  // having been hidden.
  void WasHidden();
  void WasShown(const base::Optional<RecordTabSwitchTimeRequest>&
                    record_tab_switch_time_request);

#if defined(OS_ANDROID)
  // Set the importance of widget. The importance is passed onto
  // RenderProcessHost which aggregates importance of all of its widgets.
  void SetImportance(ChildProcessImportance importance);
  ChildProcessImportance importance() const { return importance_; }

  void AddImeInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
  void RemoveImeInputEventObserver(
      RenderWidgetHost::InputEventObserver* observer) override;
#endif

  // Returns true if the RenderWidget is hidden.
  bool is_hidden() const { return is_hidden_; }

  // Called to notify the RenderWidget that its associated native window
  // got/lost focused.
  void GotFocus();
  void LostFocus();
  void LostCapture();

  // Indicates whether the RenderWidgetHost thinks it is focused.
  // This is different from RenderWidgetHostView::HasFocus() in the sense that
  // it reflects what the renderer process knows: it saves the state that is
  // sent/received.
  // RenderWidgetHostView::HasFocus() is checking whether the view is focused so
  // it is possible in some edge cases that a view was requested to be focused
  // but it failed, thus HasFocus() returns false.
  bool is_focused() const { return is_focused_; }

  // Support for focus tracking on multi-WebContents cases. This will notify all
  // renderers involved in a page about a page-level focus update. Users other
  // than WebContents and RenderWidgetHost should use Focus()/Blur().
  void SetPageFocus(bool focused);

  // Called to notify the RenderWidget that it has lost the mouse lock.
  void LostMouseLock();

  // Notifies the RenderWidget that it lost the mouse lock.
  void SendMouseLockLost();

  bool is_last_unlocked_by_target() const {
    return is_last_unlocked_by_target_;
  }

  // Notifies the RenderWidget of the current mouse cursor visibility state.
  void OnCursorVisibilityStateChanged(bool is_visible);

  // Notifies the RenderWidget when toggle fallback cursor mode on/off.
  void OnFallbackCursorModeToggled(bool is_on);

  // Notifies the RenderWidgetHost that the View was destroyed.
  void ViewDestroyed();

  // Signals if this host has forwarded a GestureScrollBegin without yet having
  // forwarded a matching GestureScrollEnd/GestureFlingStart.
  bool is_in_touchscreen_gesture_scroll() const {
    return is_in_gesture_scroll_[static_cast<int>(
        blink::WebGestureDevice::kTouchscreen)];
  }

  bool visual_properties_ack_pending_for_testing() {
    return visual_properties_ack_pending_;
  }

  // Requests the generation of a new CompositorFrame from the renderer.
  // It will return false if the renderer is not ready (e.g. there's an
  // in flight change).
  bool RequestRepaintForTesting();

  // Called after every cross-document navigation. The displayed graphics of
  // the renderer is cleared after a certain timeout if it does not produce a
  // new CompositorFrame after navigation.
  void DidNavigate();

  // Forwards the keyboard event with optional commands to the renderer. If
  // |key_event| is not forwarded for any reason, then |commands| are ignored.
  // |update_event| (if non-null) is set to indicate whether the underlying
  // event in |key_event| should be updated. |update_event| is only used on
  // aura.
  void ForwardKeyboardEventWithCommands(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency,
      const std::vector<EditCommand>* commands,
      bool* update_event = nullptr);

  // Forwards the given message to the renderer. These are called by the view
  // when it has received a message.
  void ForwardKeyboardEventWithLatencyInfo(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency) override;
  void ForwardGestureEventWithLatencyInfo(
      const blink::WebGestureEvent& gesture_event,
      const ui::LatencyInfo& latency) override;
  virtual void ForwardTouchEventWithLatencyInfo(
      const blink::WebTouchEvent& touch_event,
      const ui::LatencyInfo& latency);  // Virtual for testing.
  void ForwardMouseEventWithLatencyInfo(const blink::WebMouseEvent& mouse_event,
                                        const ui::LatencyInfo& latency);
  void ForwardWheelEventWithLatencyInfo(
      const blink::WebMouseWheelEvent& wheel_event,
      const ui::LatencyInfo& latency) override;

  // Resolves the given callback once all effects of prior input have been
  // fully realized.
  void WaitForInputProcessed(SyntheticGestureParams::GestureType type,
                             SyntheticGestureParams::GestureSourceType source,
                             base::OnceClosure callback);

  // Resolves the given callback once all effects of previously forwarded input
  // have been fully realized (i.e. resulting compositor frame has been drawn,
  // swapped, and presented).
  void WaitForInputProcessed(base::OnceClosure callback);

  // Retrieve an iterator over any RenderWidgetHosts that are immediately
  // embedded within this one. This does not return hosts that are embedded
  // indirectly (i.e. nested within embedded hosts).
  std::unique_ptr<RenderWidgetHostIterator> GetEmbeddedRenderWidgetHosts();

  // Returns an emulator for this widget. See TouchEmulator for more details.
  TouchEmulator* GetTouchEmulator();

  void SetCursor(const WebCursor& cursor);
  void ShowContextMenuAtPoint(const gfx::Point& point,
                              const ui::MenuSourceType source_type);

  // Queues a synthetic gesture for testing purposes.  Invokes the on_complete
  // callback when the gesture is finished running.
  void QueueSyntheticGesture(
      std::unique_ptr<SyntheticGesture> synthetic_gesture,
      base::OnceCallback<void(SyntheticGesture::Result)> on_complete);
  void QueueSyntheticGestureCompleteImmediately(
      std::unique_ptr<SyntheticGesture> synthetic_gesture);

  // Ensures the renderer is in a state ready to receive synthetic input. The
  // SyntheticGestureController will normally ensure this before sending the
  // first gesture; however, in some tests that may be a bad time (e.g. the
  // gesture is sent while the main thread is blocked) so this allows the
  // caller to do so manually.
  void EnsureReadyForSyntheticGestures(base::OnceClosure on_ready);

  void TakeSyntheticGestureController(RenderWidgetHostImpl* host);

  // Update the composition node of the renderer (or WebKit).
  // WebKit has a special node (a composition node) for input method to change
  // its text without affecting any other DOM nodes. When the input method
  // (attached to the browser) updates its text, the browser sends IPC messages
  // to update the composition node of the renderer.
  // (Read the comments of each function for its detail.)

  // Sets the text of the composition node.
  // This function can also update the cursor position and mark the specified
  // range in the composition node.
  // A browser should call this function:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_COMPSTR flag
  //   (on Windows);
  // * when it receives a "preedit_changed" signal of GtkIMContext (on Linux);
  // * when markedText of NSTextInput is called (on Mac).
  void ImeSetComposition(const base::string16& text,
                         const std::vector<ui::ImeTextSpan>& ime_text_spans,
                         const gfx::Range& replacement_range,
                         int selection_start,
                         int selection_end);

  // Deletes the ongoing composition if any, inserts the specified text, and
  // moves the cursor.
  // A browser should call this function or ImeFinishComposingText:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeCommitText(const base::string16& text,
                     const std::vector<ui::ImeTextSpan>& ime_text_spans,
                     const gfx::Range& replacement_range,
                     int relative_cursor_pos);

  // Finishes an ongoing composition.
  // A browser should call this function or ImeCommitText:
  // * when it receives a WM_IME_COMPOSITION message with a GCS_RESULTSTR flag
  //   (on Windows);
  // * when it receives a "commit" signal of GtkIMContext (on Linux);
  // * when insertText of NSTextInput is called (on Mac).
  void ImeFinishComposingText(bool keep_selection);

  // Cancels an ongoing composition.
  void ImeCancelComposition();

  // Whether forwarded WebInputEvents are being ignored.
  bool IsIgnoringInputEvents() const;

  bool has_touch_handler() const { return has_touch_handler_; }

  // Called when the response to a pending mouse lock request has arrived.
  // Returns true if |allowed| is true and the mouse has been successfully
  // locked.
  bool GotResponseToLockMouseRequest(bool allowed);

  void set_allow_privileged_mouse_lock(bool allow) {
    allow_privileged_mouse_lock_ = allow;
  }

  // Called when the response to a pending keyboard lock request has arrived.
  // |allowed| should be true if the current tab is in tab initiated fullscreen
  // mode.
  void GotResponseToKeyboardLockRequest(bool allowed);

  // Resets state variables related to tracking pending updates to visual
  // properties.
  void ResetSentVisualProperties();

  // When the WebContents (which acts as the Delegate) is destroyed, this object
  // may still outlive it while the renderer is shutting down. In that case the
  // delegate pointer is removed (since it would be a UAF).
  void DetachDelegate();

  // Update the renderer's cache of the screen rect of the view and window.
  void SendScreenRects();

  // Indicates whether the renderer drives the RenderWidgetHosts's size or the
  // other way around.
  bool auto_resize_enabled() { return auto_resize_enabled_; }

  // The minimum size of this renderer when auto-resize is enabled.
  const gfx::Size& min_size_for_auto_resize() const {
    return min_size_for_auto_resize_;
  }

  // The maximum size of this renderer when auto-resize is enabled.
  const gfx::Size& max_size_for_auto_resize() const {
    return max_size_for_auto_resize_;
  }

  // Don't check whether we expected a resize ack during web tests.
  static void DisableResizeAckCheckForTesting();

  InputRouter* input_router() { return input_router_.get(); }

  void SetForceEnableZoom(bool);

  // Get the BrowserAccessibilityManager for the root of the frame tree,
  BrowserAccessibilityManager* GetRootBrowserAccessibilityManager();

  // Get the BrowserAccessibilityManager for the root of the frame tree,
  // or create it if it doesn't already exist.
  BrowserAccessibilityManager* GetOrCreateRootBrowserAccessibilityManager();

  void RejectMouseLockOrUnlockIfNecessary();

  void set_renderer_initialized(bool renderer_initialized) {
    renderer_initialized_ = renderer_initialized;
  }

  // Store values received in a child frame RenderWidgetHost from a parent
  // RenderWidget, in order to pass them to the renderer and continue their
  // propagation down the RenderWidget tree.
  void SetVisualPropertiesFromParentFrame(float page_scale_factor,
                                          bool is_pinch_gesture_active,
                                          const gfx::Rect& compositor_viewport);

  // Indicates if the render widget host should track the render widget's size
  // as opposed to visa versa.
  // In main frame RenderWidgetHosts this controls the value for the frame tree.
  // In child frame RenderWidgetHosts this value comes from the parent
  // RenderWidget and should be propagated down the RenderWidgetTree.
  void SetAutoResize(bool enable,
                     const gfx::Size& min_size,
                     const gfx::Size& max_size);

  // Generates a filled in VisualProperties struct representing the current
  // properties of this widget.
  VisualProperties GetVisualProperties();

  // Sets the |visual_properties| that were sent to the renderer bundled with
  // the request to create a new RenderWidget.
  void SetInitialVisualProperties(const VisualProperties& visual_properties);

  // Pushes updated visual properties to the renderer as well as whether the
  // focused node should be scrolled into view.
  bool SynchronizeVisualProperties(bool scroll_focused_node_into_view);

  // Similar to SynchronizeVisualProperties(), but performed even if
  // |visual_properties_ack_pending_| is set.  Used to guarantee that the
  // latest visual properties are sent to the renderer before another IPC.
  void SynchronizeVisualPropertiesIgnoringPendingAck();

  // Called when we receive a notification indicating that the renderer process
  // is gone. This will reset our state so that our state will be consistent if
  // a new renderer is created.
  void RendererExited();

  // Called from a RenderFrameHost when the text selection has changed.
  void SelectionChanged(const base::string16& text,
                        uint32_t offset,
                        const gfx::Range& range);

  size_t in_flight_event_count() const { return in_flight_event_count_; }

  bool renderer_initialized() const { return renderer_initialized_; }

  bool needs_begin_frames() const { return needs_begin_frames_; }

  base::WeakPtr<RenderWidgetHostImpl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Request composition updates from RenderWidget. If |immediate_request| is
  // true, RenderWidget will respond immediately. If |monitor_updates| is true,
  // then RenderWidget sends updates for each compositor frame when there are
  // changes, or when the text selection changes inside a frame. If both fields
  // are false, RenderWidget will not send any updates. To avoid sending
  // unnecessary IPCs to RenderWidget (e.g., asking for monitor updates while
  // we are already receiving updates), when
  // |monitoring_composition_info_| == |monitor_updates| no IPC is sent to the
  // renderer unless it is for an immediate request.
  void RequestCompositionUpdates(bool immediate_request, bool monitor_updates);

  void RequestCompositorFrameSink(
      mojo::PendingReceiver<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_receiver,
      mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient>
          compositor_frame_sink_client);

  void RegisterRenderFrameMetadataObserver(
      mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
          render_frame_metadata_observer_client_receiver,
      mojo::PendingRemote<mojom::RenderFrameMetadataObserver>
          render_frame_metadata_observer);

  RenderFrameMetadataProviderImpl* render_frame_metadata_provider() {
    return &render_frame_metadata_provider_;
  }

  bool HasGestureStopped() override;

  // viz::mojom::CompositorFrameSink implementation.
  void SetNeedsBeginFrame(bool needs_begin_frame) override;
  void SetWantsAnimateOnlyBeginFrames() override;
  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time) override;
  void SubmitCompositorFrameSync(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      base::Optional<viz::HitTestRegionList> hit_test_region_list,
      uint64_t submit_time,
      const SubmitCompositorFrameSyncCallback callback) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

  // Signals that a frame with token |frame_token| was finished processing. If
  // there are any queued messages belonging to it, they will be processed.
  void DidProcessFrame(uint32_t frame_token);

  // Indicate the frame input handler is now available.
  void SetFrameInputHandler(mojom::FrameInputHandler*);
  void SetWidget(mojo::PendingRemote<mojom::Widget> widget_remote);

  viz::mojom::InputTargetClient* input_target_client() {
    return input_target_client_.get();
  }

  void SetInputTargetClient(
      mojo::Remote<viz::mojom::InputTargetClient> input_target_client);

  // InputRouterImplClient overrides.
  mojom::WidgetInputHandler* GetWidgetInputHandler() override;
  void OnImeCompositionRangeChanged(
      const gfx::Range& range,
      const std::vector<gfx::Rect>& character_bounds) override;
  void OnImeCancelComposition() override;
  bool IsWheelScrollInProgress() override;
  bool IsAutoscrollInProgress() override;
  void SetMouseCapture(bool capture) override;
  void FallbackCursorModeLockCursor(bool left,
                                    bool right,
                                    bool up,
                                    bool down) override;
  void FallbackCursorModeSetCursorVisibility(bool visible) override;
  gfx::Size GetRootWidgetViewportSize() override;

  // FrameTokenMessageQueue::Client:
  void OnInvalidFrameToken(uint32_t frame_token) override;
  void OnMessageDispatchError(const IPC::Message& message) override;
  void OnProcessSwapMessage(const IPC::Message& message) override;

  void ProgressFlingIfNeeded(base::TimeTicks current_time);
  void StopFling();
  void SetNeedsBeginFrameForFlingProgress();

  // The RenderWidgetHostImpl will keep showing the old page (for a while) after
  // navigation until the first frame of the new page arrives. This reduces
  // flicker. However, if for some reason it is known that the frames won't be
  // arriving, this call can be used for force a timeout, to avoid showing the
  // content of the old page under UI from the new page.
  void ForceFirstFrameAfterNavigationTimeout();

  void SetScreenOrientationForTesting(uint16_t angle,
                                      ScreenOrientationValues type);

  // Requests Keyboard lock.  Note: the lock may not take effect until later.
  // If |codes| has no value then all keys will be locked, otherwise only the
  // keys specified will be intercepted and routed to the web page.
  // Returns true if the lock request was successfully registered.
  bool RequestKeyboardLock(base::Optional<base::flat_set<ui::DomCode>> codes);

  // Cancels a previous keyboard lock request.
  void CancelKeyboardLock();

  // Indicates whether keyboard lock is active.
  bool IsKeyboardLocked() const;

  // Returns the keyboard layout mapping.
  base::flat_map<std::string, std::string> GetKeyboardLayoutMap();

  void DidStopFlinging();

  void GetContentRenderingTimeoutFrom(RenderWidgetHostImpl* other);

  // Called on delayed response from the renderer by either
  // 1) |hang_monitor_timeout_| (slow to ack input events) or
  // 2) NavigationHandle::OnCommitTimeout (slow to commit).
  void RendererIsUnresponsive(
      base::RepeatingClosure restart_hang_monitor_timeout);

  // Called if we know the renderer is responsive. When we currently think the
  // renderer is unresponsive, this will clear that state and call
  // NotifyRendererResponsive.
  void RendererIsResponsive();

  // Called during frame eviction to return all SurfaceIds in the frame tree.
  // Marks all views in the frame tree as evicted.
  std::vector<viz::SurfaceId> CollectSurfaceIdsForEviction();

  // This function validates a renderer's attempt to activate frames. It
  // removes one pending user activation if available and returns true;
  // otherwise, it returns false.  See comments on
  // Add/ClearPendingUserActivation() for details.
  bool RemovePendingUserActivationIfAvailable();

 protected:
  // ---------------------------------------------------------------------------
  // The following method is overridden by RenderViewHost to send upwards to
  // its delegate.

  // Callback for notification that we failed to receive any rendered graphics
  // from a newly loaded page. Used for testing.
  virtual void NotifyNewContentRenderingTimeoutForTesting() {}

  // InputDispositionHandler
  void OnWheelEventAck(const MouseWheelEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override;
  void OnTouchEventAck(const TouchEventWithLatencyInfo& event,
                       InputEventAckSource ack_source,
                       InputEventAckState ack_result) override;
  void OnGestureEventAck(const GestureEventWithLatencyInfo& event,
                         InputEventAckSource ack_source,
                         InputEventAckState ack_result) override;

  // virtual for testing.
  virtual void OnMouseEventAck(const MouseEventWithLatencyInfo& event,
                               InputEventAckSource ack_source,
                               InputEventAckState ack_result);
  // ---------------------------------------------------------------------------

  bool IsMouseLocked() const;

  // The View associated with the RenderWidgetHost. The lifetime of this object
  // is associated with the lifetime of the Render process. If the Renderer
  // crashes, its View is destroyed and this pointer becomes NULL, even though
  // render_view_host_ lives on to load another URL (creating a new View while
  // doing so).
  base::WeakPtr<RenderWidgetHostViewBase> view_;

 private:
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest,
                           DontPostponeInputEventAckTimeout);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, HideShowMessages);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, PendingUserActivationTimeout);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, RendererExitedNoDrag);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest,
                           StopAndStartInputEventAckTimeout);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest,
                           ShorterDelayInputEventAckTimeout);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest, SynchronizeVisualProperties);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest,
                           AddAndRemoveInputEventObserver);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostTest,
                           AddAndRemoveImeInputEventObserver);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, AutoResizeWithScale);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           AutoResizeWithBrowserInitiatedResize);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           ChildAllocationAcceptedInParentWhileHidden);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest, Resize);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewChildFrameTest,
                           ChildFrameAutoResizeUpdate);
  FRIEND_TEST_ALL_PREFIXES(DevToolsManagerTest,
                           NoUnresponsiveDialogInInspectedContents);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           ChildAllocationAcceptedInParent);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewAuraTest,
                           ConflictingAllocationsResolve);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewMacTest,
                           ChildAllocationAcceptedInParent);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostViewMacTest,
                           ConflictingAllocationsResolve);
  FRIEND_TEST_ALL_PREFIXES(SitePerProcessBrowserTest,
                           ResizeAndCrossProcessPostMessagePreserveOrder);
  FRIEND_TEST_ALL_PREFIXES(RenderWidgetHostInputEventRouterTest,
                           EnsureRendererDestroyedHandlesUnAckedTouchEvents);
  friend class MockRenderWidgetHost;
  friend class OverscrollNavigationOverlayTest;
  friend class RenderViewHostTester;
  friend class TestRenderViewHost;
  friend bool TestChildOrGuestAutoresize(bool,
                                         RenderProcessHost*,
                                         RenderWidgetHost*);

  // Tell this object to destroy itself. If |also_delete| is specified, the
  // destructor is called as well.
  void Destroy(bool also_delete);

  // Called by |new_content_rendering_timeout_| if a renderer has loaded new
  // content but failed to produce a compositor frame in a defined time.
  void ClearDisplayedGraphics();

  // InputRouter::SendKeyboardEvent() callbacks to this. This may be called
  // synchronously.
  void OnKeyboardEventAck(const NativeWebKeyboardEventWithLatencyInfo& event,
                          InputEventAckSource ack_source,
                          InputEventAckState ack_result);

  // IPC message handlers
  void OnClose();
  void OnUpdateScreenRectsAck();
  void OnRequestSetBounds(const gfx::Rect& bounds);
  void OnSetTooltipText(const base::string16& tooltip_text,
                        blink::WebTextDirection text_direction_hint);
  void OnSetCursor(const WebCursor& cursor);
  void OnAutoscrollStart(const gfx::PointF& position);
  void OnAutoscrollFling(const gfx::Vector2dF& velocity);
  void OnAutoscrollEnd();
  void OnTextInputStateChanged(const TextInputState& params);

  void OnLockMouse(bool user_gesture,
                   bool privileged,
                   bool request_unadjusted_movement);
  void OnUnlockMouse();
  void OnSelectionBoundsChanged(
      const WidgetHostMsg_SelectionBounds_Params& params);
  void OnSetNeedsBeginFrames(bool needs_begin_frames);
  void OnStartDragging(const DropData& drop_data,
                       blink::WebDragOperationsMask operations_allowed,
                       const SkBitmap& bitmap,
                       const gfx::Vector2d& bitmap_offset_in_dip,
                       const DragEventSourceInfo& event_info);
  void OnUpdateDragCursor(blink::WebDragOperation current_op);
  void OnFrameSwapMessagesReceived(uint32_t frame_token,
                                   std::vector<IPC::Message> messages);
  void OnForceRedrawComplete(int snapshot_id);
  void OnFirstVisuallyNonEmptyPaint();
  void OnCommitAndDrawCompositorFrame();
  void OnHasTouchEventHandlers(bool has_handlers);
  void OnIntrinsicSizingInfoChanged(blink::WebIntrinsicSizingInfo info);
  void OnAnimateDoubleTapZoomInMainFrame(const gfx::Point& point,
                                         const gfx::Rect& rect_to_zoom);
  void OnZoomToFindInPageRectInMainFrame(const gfx::Rect& rect_to_zoom);

  // Called when visual properties have changed in the renderer.
  void DidUpdateVisualProperties(const cc::RenderFrameMetadata& metadata);

  // Returns true if the |new_visual_properties| differs from
  // |old_page_visual_properties| in a way that indicates a size changed.
  static bool DidVisualPropertiesSizeChange(
      const VisualProperties& old_visual_properties,
      const VisualProperties& new_visual_properties);

  // Returns true if the new visual properties requires an ack from a
  // synchronization message.
  static bool DoesVisualPropertiesNeedAck(
      const std::unique_ptr<VisualProperties>& old_visual_properties,
      const VisualProperties& new_visual_properties);

  // Returns true if |old_visual_properties| is out of sync with
  // |new_visual_properties|.
  static bool StoredVisualPropertiesNeedsUpdate(
      const std::unique_ptr<VisualProperties>& old_visual_properties,
      const VisualProperties& new_visual_properties);

  // Give key press listeners a chance to handle this key press. This allow
  // widgets that don't have focus to still handle key presses.
  bool KeyPressListenersHandleEvent(const NativeWebKeyboardEvent& event);

  // InputRouterClient
  InputEventAckState FilterInputEvent(
      const blink::WebInputEvent& event,
      const ui::LatencyInfo& latency_info) override;
  void IncrementInFlightEventCount() override;
  void DecrementInFlightEventCount(InputEventAckSource ack_source) override;
  void DidOverscroll(const ui::DidOverscrollParams& params) override;
  void DidStartScrollingViewport() override;
  void OnSetWhiteListedTouchAction(
      cc::TouchAction white_listed_touch_action) override {}

  // Dispatch input events with latency information
  void DispatchInputEventWithLatencyInfo(const blink::WebInputEvent& event,
                                         ui::LatencyInfo* latency);

  void WindowSnapshotReachedScreen(int snapshot_id);

  void OnSnapshotFromSurfaceReceived(int snapshot_id,
                                     int retry_count,
                                     const SkBitmap& bitmap);

  void OnSnapshotReceived(int snapshot_id, gfx::Image image);

  // Called by the RenderProcessHost to handle the case when the process
  // changed its state of being blocked.
  void RenderProcessBlockedStateChanged(bool blocked);

  // 1. Grants permissions to URL (if any)
  // 2. Grants permissions to filenames
  // 3. Grants permissions to file system files.
  // 4. Register the files with the IsolatedContext.
  void GrantFileAccessFromDropData(DropData* drop_data);

  // Starts a hang monitor timeout. If there's already a hang monitor timeout
  // the new one will only fire if it has a shorter delay than the time
  // left on the existing timeouts.
  void StartInputEventAckTimeout(base::TimeDelta delay);

  // Stops all existing hang monitor timeouts and assumes the renderer is
  // responsive.
  void StopInputEventAckTimeout();

  // Implementation of |hang_monitor_restarter| callback passed to
  // RenderWidgetHostDelegate::RendererUnresponsive if the unresponsiveness
  // was noticed because of input event ack timeout.
  void RestartInputEventAckTimeoutIfNecessary();

  // Called by |input_event_ack_timeout_| when an input event timed out without
  // getting an ack from the renderer.
  void OnInputEventAckTimeout();

  void SetupInputRouter();

  // Start intercepting system keyboard events.
  bool LockKeyboard();

  // Stop intercepting system keyboard events.
  void UnlockKeyboard();

#if defined(OS_MACOSX)
  device::mojom::WakeLock* GetWakeLock();
#endif

  // RenderFrameMetadataProvider::Observer implementation.
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation() override;
  void OnRenderFrameSubmission() override {}
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override;

  // Returns a pointer to the touch emulator serving this host, but only if it
  // already exists; calling this function will not force creation of a
  // TouchEmulator.
  TouchEmulator* GetExistingTouchEmulator();

  void CreateSyntheticGestureControllerIfNecessary();

  // The following functions are used to keep track of pending user activation
  // events, which are input events (e.g., mousedown or keydown) that allow a
  // renderer to gain user activation.  AddPendingUserActivation() increments
  // |pending_user_activation_counter_| and sets a timer, which allows the
  // renderer to claim user activation within
  // |kActivationNotificationExpireTime| ms.  ClearPendingUserActivation()
  // clears the counter and is called after navigations or timeouts.
  void AddPendingUserActivation(const blink::WebInputEvent& event);
  void ClearPendingUserActivation();

  // An expiry time for resetting the pending_user_activation_timer_.
  static const base::TimeDelta kActivationNotificationExpireTime;

  // true if a renderer has once been valid. We use this flag to display a sad
  // tab only when we lose our renderer and not if a paint occurs during
  // initialization.
  bool renderer_initialized_ = false;

  // True if |Destroy()| has been called.
  bool destroyed_ = false;

  // Our delegate, which wants to know mainly about keyboard events.
  // It will remain non-NULL until DetachDelegate() is called.
  RenderWidgetHostDelegate* delegate_;

  // The delegate of the owner of this object.
  // This member is non-null if and only if this RenderWidgetHost is associated
  // with a main frame RenderWidget.
  RenderWidgetHostOwnerDelegate* owner_delegate_ = nullptr;

  // Created during construction and guaranteed never to be NULL, but its
  // channel may be NULL if the renderer crashed, so one must always check that.
  RenderProcessHost* const process_;

  // The ID of the corresponding object in the Renderer Instance.
  const int routing_id_;

  // The clock used; overridable for tests.
  const base::TickClock* clock_;

  // Indicates whether a page is loading or not.
  bool is_loading_ = false;

  // Indicates whether a page is hidden or not. Need to call
  // process_->UpdateClientPriority when this value changes.
  bool is_hidden_;

  // For a widget that does not have an associated RenderFrame/View, assume it
  // is depth 1, ie just below the root widget.
  unsigned int frame_depth_ = 1u;

  // Indicates that widget has a frame that intersects with the viewport. Note
  // this is independent of |is_hidden_|. For widgets not associated with
  // RenderFrame/View, assume false.
  bool intersects_viewport_ = false;

#if defined(OS_ANDROID)
  // Tracks the current importance of widget.
  ChildProcessImportance importance_ = ChildProcessImportance::NORMAL;
#endif

  // True when waiting for visual_properties_ack.
  bool visual_properties_ack_pending_ = false;

  // Visual properties that were most recently sent to the renderer.
  std::unique_ptr<VisualProperties> old_visual_properties_;

  // True if the render widget host should track the render widget's size as
  // opposed to visa versa.
  bool auto_resize_enabled_ = false;

  // The minimum size for the render widget if auto-resize is enabled.
  gfx::Size min_size_for_auto_resize_;

  // The maximum size for the render widget if auto-resize is enabled.
  gfx::Size max_size_for_auto_resize_;

  // These properties are propagated down the RenderWidget tree from the main
  // frame to a child frame RenderWidgetHost. They are not used on a top-level
  // RenderWidgetHost. The child frame RenderWidgetHost stores these values to
  // pass them to the renderer, instead of computing them for itself. It
  // collects them and passes them though WidgetMsg_UpdateVisualProperties so
  // that the renderer receives updates in an atomic fashion along with a
  // synchronization token for the compositor in a LocalSurfaceIdAllocation.
  struct MainFramePropagationProperties {
    // The page-scale factor of the main-frame.
    float page_scale_factor = 1.f;

    // True when the renderer is currently undergoing a pinch-zoom gesture.
    bool is_pinch_gesture_active = false;

    gfx::Rect compositor_viewport;
  } properties_from_parent_frame_;

  bool waiting_for_screen_rects_ack_ = false;
  gfx::Rect last_view_screen_rect_;
  gfx::Rect last_window_screen_rect_;

  // Keyboard event listeners.
  std::vector<KeyPressEventCallback> key_press_event_callbacks_;

  // Mouse event callbacks.
  std::vector<MouseEventCallback> mouse_event_callbacks_;

  // Input event callbacks.
  base::ObserverList<RenderWidgetHost::InputEventObserver>::Unchecked
      input_event_observers_;

#if defined(OS_ANDROID)
  // Ime input event callbacks. This is separated from input_event_observers_,
  // because not all text events are triggered by input events on Android.
  base::ObserverList<RenderWidgetHost::InputEventObserver>::Unchecked
      ime_input_event_observers_;
#endif

  // The observers watching us.
  base::ObserverList<RenderWidgetHostObserver> observers_;

  // This is true if the renderer is currently unresponsive.
  bool is_unresponsive_ = false;

  // This value denotes the number of input events yet to be acknowledged
  // by the renderer.
  int in_flight_event_count_ = 0;

  // Flag to detect recursive calls to GetBackingStore().
  bool in_get_backing_store_ = false;

  // Used for UMA histogram logging to measure the time for a repaint view
  // operation to finish.
  base::TimeTicks repaint_start_time_;

  // Set when we update the text direction of the selected input element.
  bool text_direction_updated_ = false;
  blink::WebTextDirection text_direction_ = blink::kWebTextDirectionLeftToRight;

  // Indicates if Char and KeyUp events should be suppressed or not. Usually all
  // events are sent to the renderer directly in sequence. However, if a
  // RawKeyDown event was handled by PreHandleKeyboardEvent() or
  // KeyPressListenersHandleEvent(), e.g. as an accelerator key, then the
  // RawKeyDown event is not sent to the renderer, and the following sequence of
  // Char and KeyUp events should also not be sent. Otherwise the renderer will
  // see only the Char and KeyUp events and cause unexpected behavior. For
  // example, pressing alt-2 may let the browser switch to the second tab, but
  // the Char event generated by alt-2 may also activate a HTML element if its
  // accesskey happens to be "2", then the user may get confused when switching
  // back to the original tab, because the content may already have changed.
  bool suppress_events_until_keydown_ = false;

  bool pending_mouse_lock_request_ = false;
  bool allow_privileged_mouse_lock_ = false;
  bool mouse_lock_raw_movement_ = false;
  // Stores the keyboard keys to lock while waiting for a pending lock request.
  base::Optional<base::flat_set<ui::DomCode>> keyboard_keys_to_lock_;
  bool keyboard_lock_requested_ = false;
  bool keyboard_lock_allowed_ = false;

  // Used when locking to indicate when a target application has voluntarily
  // unlocked and desires to relock the mouse. If the mouse is unlocked due
  // to ESC being pressed by the user, this will be false.
  bool is_last_unlocked_by_target_ = false;

  // Keeps track of whether the webpage has any touch event handler. If it does,
  // then touch events are sent to the renderer. Otherwise, the touch events are
  // not sent to the renderer.
  bool has_touch_handler_ = false;

  // TODO(wjmaclean) Remove the code for supporting resending gesture events
  // when WebView transitions to OOPIF and BrowserPlugin is removed.
  // http://crbug.com/533069
  bool is_in_gesture_scroll_[static_cast<int>(
                                 blink::WebGestureDevice::kMaxValue) +
                             1] = {false};
  bool is_in_touchpad_gesture_fling_ = false;

  std::unique_ptr<SyntheticGestureController> synthetic_gesture_controller_;

  // Receives and handles all input events.
  std::unique_ptr<InputRouter> input_router_;

  std::unique_ptr<TimeoutMonitor> input_event_ack_timeout_;
  base::TimeTicks input_event_ack_start_time_;

  std::unique_ptr<base::CallbackList<void(bool)>::Subscription>
      render_process_blocked_state_changed_subscription_;

  std::unique_ptr<TimeoutMonitor> new_content_rendering_timeout_;

  RenderWidgetHostLatencyTracker latency_tracker_;

  int next_browser_snapshot_id_ = 1;
  using PendingSnapshotMap = std::map<int, GetSnapshotFromBrowserCallback>;
  PendingSnapshotMap pending_browser_snapshots_;
  PendingSnapshotMap pending_surface_browser_snapshots_;

  // Indicates whether a RenderFramehost has ownership, in which case this
  // object does not self destroy.
  bool owned_by_render_frame_host_ = false;

  // Indicates whether this RenderWidgetHost thinks is focused. This is trying
  // to match what the renderer process knows. It is different from
  // RenderWidgetHostView::HasFocus in that in that the focus request may fail,
  // causing HasFocus to return false when is_focused_ is true.
  bool is_focused_ = false;

  // Whether the view should send begin frame messages to its render widget.
  // This is state that may arrive before the view has been set and that must be
  // consistent with the state in the renderer, so this host handles it.
  bool needs_begin_frames_ = false;

  // This is used to make sure that when the fling controller sets
  // needs_begin_frames_ it doesn't get overriden by the renderer.
  bool browser_fling_needs_begin_frame_ = false;

  // This value indicates how long to wait before we consider a renderer hung.
  base::TimeDelta hung_renderer_delay_;

  // This value indicates how long to wait for a new compositor frame from a
  // renderer process before clearing any previously displayed content.
  base::TimeDelta new_content_rendering_delay_;

  // When true, the RenderWidget is regularly sending updates regarding
  // composition info. It should only be true when there is a focused editable
  // node.
  bool monitoring_composition_info_ = false;

#if defined(OS_MACOSX)
  mojo::Remote<device::mojom::WakeLock> wake_lock_;
#endif

  mojo::Receiver<viz::mojom::CompositorFrameSink>
      compositor_frame_sink_receiver_;
  mojo::Remote<viz::mojom::CompositorFrameSinkClient>
      renderer_compositor_frame_sink_;

  // Stash a request to create a CompositorFrameSink if it arrives before
  // we have a view. This is only used if |enable_viz_| is true.
  base::OnceCallback<void(const viz::FrameSinkId&)> create_frame_sink_callback_;

  std::unique_ptr<FrameTokenMessageQueue> frame_token_message_queue_;

  bool enable_viz_ = false;

  // If the |associated_widget_input_handler_| is set it should always be
  // used to ensure in order delivery of related messages that may occur
  // at the frame input level; see FrameInputHandler. Note that when the
  // RWHI wraps a WebPagePopup widget it will only have a
  // a |widget_input_handler_|.
  mojo::AssociatedRemote<mojom::WidgetInputHandler>
      associated_widget_input_handler_;
  mojo::Remote<mojom::WidgetInputHandler> widget_input_handler_;
  mojo::Remote<viz::mojom::InputTargetClient> input_target_client_;

  base::Optional<uint16_t> screen_orientation_angle_for_testing_;
  base::Optional<ScreenOrientationValues> screen_orientation_type_for_testing_;

  // When the viz display compositor is in the browser process, this is used to
  // register and unregister the bitmaps (stored in |owned_bitmaps_| reported to
  // this class from the renderer.
  viz::ServerSharedBitmapManager* shared_bitmap_manager_ = nullptr;
  // The set of SharedBitmapIds that have been reported as allocated to this
  // interface. On closing this interface, the display compositor should drop
  // ownership of the bitmaps with these ids to avoid leaking them. This is only
  // used when SharedBitmaps are reported to this class because the display
  // compositor is in the browser process.
  std::set<viz::SharedBitmapId> owned_bitmaps_;

  bool force_enable_zoom_ = false;

  RenderFrameMetadataProviderImpl render_frame_metadata_provider_;
  bool surface_id_allocation_suppressed_ = false;

  const viz::FrameSinkId frame_sink_id_;

  std::unique_ptr<FlingSchedulerBase> fling_scheduler_;

  bool sent_autoscroll_scroll_begin_ = false;
  gfx::PointF autoscroll_start_position_;

  // True when the cursor has entered the autoscroll mode. A GSB is not
  // necessarily sent yet.
  bool autoscroll_in_progress_ = false;

  // Counter for possible-activation-triggering input event.
  int pending_user_activation_counter_ = 0;
  // This timer resets |pending_user_activation_counter_| after a short delay.
  // See comments on Add/ClearPendingUserActivation().
  base::OneShotTimer pending_user_activation_timer_;

  std::unique_ptr<PeakGpuMemoryTracker> scroll_peak_gpu_mem_tracker_;

  base::WeakPtrFactory<RenderWidgetHostImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_IMPL_H_

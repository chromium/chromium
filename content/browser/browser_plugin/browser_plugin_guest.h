// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A BrowserPluginGuest is the browser side of a browser <--> embedder
// renderer channel. A BrowserPlugin (a WebPlugin) is on the embedder
// renderer side of browser <--> embedder renderer communication.
//
// BrowserPluginGuest lives on the UI thread of the browser process. Any
// messages about the guest render process that the embedder might be interested
// in receiving should be listened for here.
//
// BrowserPluginGuest is a WebContentsObserver for the guest WebContents.
// BrowserPluginGuest operates under the assumption that the guest will be
// accessible through only one RenderViewHost for the lifetime of
// the guest WebContents. Thus, cross-process navigation is not supported.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/common/surfaces/local_surface_id_allocation.h"
#include "components/viz/common/surfaces/scoped_surface_id_allocator.h"
#include "content/browser/renderer_host/input_event_shim.h"
#include "content/common/edit_command.h"
#include "content/public/browser/browser_plugin_guest_delegate.h"
#include "content/public/browser/guest_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/input_event_ack_state.h"
#include "content/public/common/screen_info.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_drag_status.h"
#include "third_party/blink/public/web/web_ime_text_span.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/rect.h"

struct BrowserPluginHostMsg_Attach_Params;
struct BrowserPluginHostMsg_SetComposition_Params;

#if defined(OS_MACOSX)
struct FrameHostMsg_ShowPopup_Params;
#endif

namespace gfx {
class Range;
}  // namespace gfx

namespace cc {
class RenderFrameMetadata;
}  // namespace cc

namespace viz {
class LocalSurfaceIdAllocation;
}  // namespace viz

namespace content {

class BrowserPluginGuestManager;
class RenderFrameHostImpl;
class RenderViewHostImpl;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class RenderWidgetHostViewBase;
class SiteInstance;
struct DropData;
struct FrameVisualProperties;
struct ScreenInfo;
struct TextInputState;

// A browser plugin guest provides functionality for WebContents to operate in
// the guest role and implements guest-specific overrides for ViewHostMsg_*
// messages.
//
// When a guest is initially created, it is in an unattached state. That is,
// it is not visible anywhere and has no embedder WebContents assigned.
// A BrowserPluginGuest is said to be "attached" if it has an embedder.
// A BrowserPluginGuest can also create a new unattached guest via
// CreateNewWindow. The newly created guest will live in the same partition,
// which means it can share storage and can script this guest.
//
// Note: in --site-per-process, all IPCs sent out from this class will be
// dropped on the floor since we don't have a BrowserPlugin.
class CONTENT_EXPORT BrowserPluginGuest : public GuestHost,
                                          public WebContentsObserver {
 public:
  ~BrowserPluginGuest() override;

  // The WebContents passed into the factory method here has not been
  // initialized yet and so it does not yet hold a SiteInstance.
  // BrowserPluginGuest must be constructed and installed into a WebContents
  // prior to its initialization because WebContents needs to determine what
  // type of WebContentsView to construct on initialization. The content
  // embedder needs to be aware of |guest_site_instance| on the guest's
  // construction and so we pass it in here.
  //
  // After this, a new BrowserPluginGuest is created with ownership transferred
  // into the |web_contents|.
  static void CreateInWebContents(WebContentsImpl* web_contents,
                                  BrowserPluginGuestDelegate* delegate);

  // Returns whether the given WebContents is a BrowserPlugin guest.
  static bool IsGuest(WebContentsImpl* web_contents);

  // Returns whether the given RenderviewHost is a BrowserPlugin guest.
  static bool IsGuest(RenderViewHostImpl* render_view_host);

  // BrowserPluginGuest::Init is called after the associated guest WebContents
  // initializes. If this guest cannot navigate without being attached to a
  // container, then this call is a no-op. For guest types that can be
  // navigated, this call adds the associated RenderWdigetHostViewGuest to the
  // view hierarchy and sets up the appropriate
  // blink::mojom::RendererPreferences so that this guest can navigate and
  // resize offscreen.
  void Init();

  // Returns an InputEventShim if this BrowserPluginGuest needs to intercept
  // input events normally handled by a RenderWidgetHost.
  InputEventShim* GetInputEventShim();

  // Returns a WeakPtr to this BrowserPluginGuest.
  base::WeakPtr<BrowserPluginGuest> AsWeakPtr();

  // Sets the focus state of the current RenderWidgetHostView.
  void SetFocus(RenderWidgetHost* rwh,
                bool focused,
                blink::WebFocusType focus_type);

  // Sets the lock state of the pointer. Returns true if |allowed| is true and
  // the mouse has been successfully locked.
  bool LockMouse(bool allowed);

  // Return true if the mouse is locked.
  bool mouse_locked() const { return mouse_locked_; }

  // Called when the embedder WebContents changes visibility.
  void EmbedderVisibilityChanged(Visibility visibility);

  // Creates a new guest WebContentsImpl with the provided |params| with |this|
  // as the |opener|.
  WebContentsImpl* CreateNewGuestWindow(
      const WebContents::CreateParams& params);

  // Returns the identifier that uniquely identifies a browser plugin guest
  // within an embedder.
  int browser_plugin_instance_id() const { return browser_plugin_instance_id_; }

  // Returns the ScreenInfo used by the guest to render.
  const ScreenInfo& screen_info() const { return screen_info_; }

  // Returns the current rect used by the guest to render.
  const gfx::Rect& frame_rect() const { return frame_rect_; }

  bool OnMessageReceivedFromEmbedder(const IPC::Message& message);

  WebContentsImpl* embedder_web_contents() const {
    return attached_ ? owner_web_contents_ : nullptr;
  }

  // Returns the embedder's RenderWidgetHostView if it is available.
  // Returns nullptr otherwise.
  RenderWidgetHostView* GetOwnerRenderWidgetHostView();

  // Returns the embedder frame.
  RenderFrameHostImpl* GetEmbedderFrame() const;

  bool focused() const { return focused_; }
  bool visible() const { return guest_visible_; }

  // Returns the viz::LocalSurfaceIdAllocation propagated from the parent to be
  // used by this guest.
  const viz::LocalSurfaceIdAllocation& local_surface_id_allocation() const {
    return local_surface_id_allocation_;
  }

  bool is_in_destruction() { return is_in_destruction_; }

  void UpdateVisibility();

  BrowserPluginGuestManager* GetBrowserPluginGuestManager() const;

  void EnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  void DisableAutoResize();
  void DidUpdateVisualProperties(const cc::RenderFrameMetadata& metadata);

  // Methods to handle events from InputEventShim.
  void DidSetHasTouchEventHandlers(bool accept);
  void DidTextInputStateChange(const TextInputState& params);
  void DidLockMouse(bool user_gesture, bool privileged);
  void DidUnlockMouse();

  // WebContentsObserver implementation.
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  void RenderViewReady() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool OnMessageReceived(const IPC::Message& message,
                         RenderFrameHost* render_frame_host) override;

  // GuestHost implementation.
  int LoadURLWithParams(
      const NavigationController::LoadURLParams& load_params) override;
  void SizeContents(const gfx::Size& new_size) override;
  void WillDestroy() override;

  // Exposes the protected web_contents() from WebContentsObserver.
  WebContentsImpl* GetWebContents() const;

  gfx::Point GetScreenCoordinates(const gfx::Point& relative_position) const;

  // Helper to send messages to embedder. If this guest is not yet attached,
  // then IPCs will be queued until attachment.
  void SendMessageToEmbedder(std::unique_ptr<IPC::Message> msg);

  // Returns whether the guest is attached to an embedder.
  bool attached() const { return attached_; }

  // Attaches this BrowserPluginGuest to the provided |embedder_web_contents|
  // and initializes the guest with the provided |params|. Attaching a guest
  // to an embedder implies that this guest's lifetime is no longer managed
  // by its opener, and it can begin loading resources.
  void Attach(int browser_plugin_instance_id,
              WebContentsImpl* embedder_web_contents,
              const BrowserPluginHostMsg_Attach_Params& params);

  // Returns whether BrowserPluginGuest is interested in receiving the given
  // |message|.
  static bool ShouldForwardToBrowserPluginGuest(const IPC::Message& message);

  void DragSourceEndedAt(float client_x,
                         float client_y,
                         float screen_x,
                         float screen_y,
                         blink::WebDragOperation operation);

  // Called when the drag started by this guest ends at an OS-level.
  void EmbedderSystemDragEnded();
  void EndSystemDragIfApplicable();

  void RespondToPermissionRequest(int request_id,
                                  bool should_allow,
                                  const std::string& user_input);

  void PointerLockPermissionResponse(bool allow);

  void ResendEventToEmbedder(const blink::WebInputEvent& event);

  // TODO(ekaramad): Remove this once https://crbug.com/642826 is resolved.
  bool can_use_cross_process_frames() const {
    return can_use_cross_process_frames_;
  }

  gfx::Point GetCoordinatesInEmbedderWebContents(
      const gfx::Point& relative_point);

 protected:

  // BrowserPluginGuest is a WebContentsObserver of |web_contents| and
  // |web_contents| has to stay valid for the lifetime of BrowserPluginGuest.
  // Constructor protected for testing.
  BrowserPluginGuest(bool has_render_view,
                     WebContentsImpl* web_contents,
                     BrowserPluginGuestDelegate* delegate);

  void set_attached_for_test(bool attached) {
    attached_ = attached;
  }

 private:
  class EmbedderVisibilityObserver;

  // InputEventShim implementation.
  class InputEventShimImpl : public InputEventShim {
   public:
    explicit InputEventShimImpl(BrowserPluginGuest* browser_plugin_guest);
    ~InputEventShimImpl() override;

    void DidSetHasTouchEventHandlers(bool accept) override;
    void DidTextInputStateChange(const TextInputState& params) override;
    void DidLockMouse(bool user_gesture, bool privileged) override;
    void DidUnlockMouse() override;

   private:
    BrowserPluginGuest* browser_plugin_guest_;
  };

  // The RenderWidgetHostImpl corresponding to the owner frame of BrowserPlugin.
  RenderWidgetHostImpl* GetOwnerRenderWidgetHost() const;

  void InitInternal(const BrowserPluginHostMsg_Attach_Params& params,
                    WebContentsImpl* owner_web_contents);

  // Message handlers for messages from embedder.
  void OnDetach(int instance_id);
  // Handles drag events from the embedder.
  // When dragging, the drag events go to the embedder first, and if the drag
  // happens on the browser plugin, then the plugin sends a corresponding
  // drag-message to the guest. This routes the drag-message to the guest
  // renderer.
  void OnDragStatusUpdate(int instance_id,
                          blink::WebDragStatus drag_status,
                          const DropData& drop_data,
                          blink::WebDragOperationsMask drag_mask,
                          const gfx::PointF& location);
  // Instructs the guest to execute an edit command decoded in the embedder.
  void OnExecuteEditCommand(int instance_id,
                            const std::string& command);

  void OnLockMouseAck(int instance_id, bool succeeded);
  // Resizes the guest's web contents.
  void OnSetFocus(int instance_id,
                  bool focused,
                  blink::WebFocusType focus_type);
  // Sets the name of the guest so that other guests in the same partition can
  // access it.
  void OnSetName(int instance_id, const std::string& name);
  // Updates the size state of the guest.
  void OnSetEditCommandsForNextKeyEvent(
      int instance_id,
      const std::vector<EditCommand>& edit_commands);
  // The guest WebContents is visible if both its embedder is visible and
  // the browser plugin element is visible. If either one is not then the
  // WebContents is marked as hidden. A hidden WebContents will consume
  // fewer GPU and CPU resources.
  //
  // When every WebContents in a RenderProcessHost is hidden, it will lower
  // the priority of the process (see
  // RenderProcessHostImpl::UpdateClientPriority).
  //
  // It will also send a message to the guest renderer process to cleanup
  // resources such as dropping back buffers and adjusting memory limits (if in
  // compositing mode, see CCLayerTreeHost::setVisible).
  //
  // Additionally, it will slow down Javascript execution and garbage
  // collection. See RenderThreadImpl::IdleHandler (executed when hidden) and
  // RenderThreadImpl::IdleHandlerInForegroundTab (executed when visible).
  void OnSetVisibility(int instance_id, bool visible);
  void OnUnlockMouseAck(int instance_id);
  void OnSynchronizeVisualProperties(
      int instance_id,
      const FrameVisualProperties& visual_properties);

  void OnImeSetComposition(
      int instance_id,
      const BrowserPluginHostMsg_SetComposition_Params& params);
  void OnImeCommitText(int instance_id,
                       const base::string16& text,
                       const std::vector<blink::WebImeTextSpan>& ime_text_spans,
                       const gfx::Range& replacement_range,
                       int relative_cursor_pos);
  void OnImeFinishComposingText(int instance_id, bool keep_selection);
  void OnExtendSelectionAndDelete(int instance_id, int before, int after);

  // Message handlers for messages from guest.
  void OnHandleInputEventAck(
      blink::WebInputEvent::Type event_type,
      InputEventAckState ack_result);
#if defined(OS_MACOSX)
  // On MacOS X popups are painted by the browser process. We handle them here
  // so that they are positioned correctly.
  void OnShowPopup(RenderFrameHost* render_frame_host,
                   const FrameHostMsg_ShowPopup_Params& params);
#endif
  void OnShowWidget(int widget_route_id, const gfx::Rect& initial_rect);
  void OnTakeFocus(bool reverse);
  void OnUpdateFrameName(int frame_id,
                         bool is_top_level,
                         const std::string& name);

  // Called when WillAttach is complete.
  void OnWillAttachComplete(WebContentsImpl* embedder_web_contents,
                            const BrowserPluginHostMsg_Attach_Params& params);

  // Returns identical message with current browser_plugin_instance_id() if
  // the input was created with browser_plugin::kInstanceIdNone, else it returns
  // the input message unmodified. If no current browser_plugin_instance_id()
  // is set, or anything goes wrong, the input message is returned.
  std::unique_ptr<IPC::Message> UpdateInstanceIdIfNecessary(
      std::unique_ptr<IPC::Message> msg) const;

  // Forwards all messages from the |pending_messages_| queue to the embedder.
  void SendQueuedMessages();

  void SendTextInputTypeChangedToView(RenderWidgetHostViewBase* guest_rwhv);

  // Creates, if necessary, and returns the routing ID of a render view for the
  // guest in the owner's renderer process.
  int GetGuestRenderViewRoutingID();

  // The last tooltip that was set with SetTooltipText().
  base::string16 current_tooltip_text_;

  InputEventShimImpl input_event_shim_impl_;

  std::unique_ptr<EmbedderVisibilityObserver> embedder_visibility_observer_;
  WebContentsImpl* owner_web_contents_;

  // Indicates whether this guest has been attached to a container.
  bool attached_;

  // An identifier that uniquely identifies a browser plugin within an embedder.
  int browser_plugin_instance_id_;
  gfx::Rect frame_rect_;
  bool focused_;
  bool mouse_locked_;
  bool pending_lock_request_;
  bool guest_visible_;
  Visibility embedder_visibility_;
  // Whether the browser plugin is inside a plugin document.
  bool is_full_page_plugin_;

  // Indicates that this BrowserPluginGuest has associated renderer-side state.
  // This is used to determine whether or not to create a new RenderView when
  // this guest is attached. A BrowserPluginGuest would have renderer-side state
  // prior to attachment if it is created via a call to window.open and
  // maintains a JavaScript reference to its opener.
  bool has_render_view_;

  bool is_in_destruction_;

  // BrowserPluginGuest::Init can only be called once. This flag allows it to
  // exit early if it's already been called.
  bool initialized_;

  // Text input type states.
  // Using scoped_ptr to avoid including the header file: view_messages.h.
  std::unique_ptr<const TextInputState> last_text_input_state_;

  // The is the routing ID for a swapped out RenderView for the guest
  // WebContents in the embedder's process.
  int guest_render_view_routing_id_;
  // Last seen state of drag status update.
  blink::WebDragStatus last_drag_status_;
  // Whether or not our embedder has seen a SystemDragEnded() call.
  bool seen_embedder_system_drag_ended_;
  // Whether or not our embedder has seen a DragSourceEndedAt() call.
  bool seen_embedder_drag_source_ended_at_;

  // Ignore the URL dragged into guest that is coming from guest.
  bool ignore_dragged_url_;

  // This is a queue of messages that are destined to be sent to the embedder
  // once the guest is attached to a particular embedder.
  base::circular_deque<std::unique_ptr<IPC::Message>> pending_messages_;

  BrowserPluginGuestDelegate* const delegate_;

  // Whether or not this BrowserPluginGuest can use cross process frames. This
  // means when we have --use-cross-process-frames-for-guests on, the
  // WebContents associated with this BrowserPluginGuest has OOPIF structure.
  bool can_use_cross_process_frames_;

  viz::LocalSurfaceIdAllocation local_surface_id_allocation_;
  ScreenInfo screen_info_;
  double zoom_level_ = 0.0;
  uint32_t capture_sequence_number_ = 0u;

  // Weak pointer used to ask GeolocationPermissionContext about geolocation
  // permission.
  base::WeakPtrFactory<BrowserPluginGuest> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGuest);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GUEST_H_

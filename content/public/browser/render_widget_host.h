// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/input_event_ack_source.h"
#include "content/public/common/input_event_ack_state.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sender.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace gfx {
class Point;
}

namespace ui {
class LatencyInfo;
}

namespace viz {
class FrameSinkId;
}

namespace content {

struct CursorInfo;
class RenderProcessHost;
class RenderWidgetHostIterator;
class RenderWidgetHostObserver;
class RenderWidgetHostView;
struct ScreenInfo;

// A RenderWidgetHost manages the browser side of a browser<->renderer
// HWND connection.  The HWND lives in the browser process, and
// windows events are sent over IPC to the corresponding object in the
// renderer.  The renderer paints into shared memory, which we
// transfer to a backing store and blit to the screen when Windows
// sends us a WM_PAINT message.
//
// How Shutdown Works
//
// There are two situations in which this object, a RenderWidgetHost, can be
// instantiated:
//
// 1. By a WebContents as the communication conduit for a rendered web page.
//    The WebContents instantiates a derived class: RenderViewHost.
// 2. By a WebContents as the communication conduit for a select widget. The
//    WebContents instantiates the RenderWidgetHost directly.
//
// For every WebContents there are several objects in play that need to be
// properly destroyed or cleaned up when certain events occur.
//
// - WebContents - the WebContents itself, and its associated HWND.
// - RenderViewHost - representing the communication conduit with the child
//   process.
// - RenderWidgetHostView - the view of the web page content, message handler,
//   and plugin root.
//
// Normally, the WebContents contains a child RenderWidgetHostView that renders
// the contents of the loaded page. It has a WS_CLIPCHILDREN style so that it
// does no painting of its own.
//
// The lifetime of the RenderWidgetHostView is tied to the render process. If
// the render process dies, the RenderWidgetHostView goes away and all
// references to it must become nullptr.
//
// RenderViewHost (an owner delegate for RenderWidgetHost) is the conduit used
// to communicate with the RenderView and is owned by the WebContents. If the
// render process crashes, the RenderViewHost remains and restarts the render
// process if needed to continue navigation.
//
// Some examples of how shutdown works:
//
// For a WebContents, its Destroy method tells the RenderViewHost to
// shut down the render process and die.
//
// When the render process is destroyed it destroys the View: the
// RenderWidgetHostView, which destroys its HWND and deletes that object.
//
// For select popups, the situation is a little different. The RenderWidgetHost
// associated with the select popup owns the view and itself (is responsible
// for destroying itself when the view is closed). The WebContents's only
// responsibility with select popups is to create them when it is told to. When
// the View is destroyed via an IPC message (triggered when WebCore destroys
// the popup, e.g. if the user selects one of the options), or because
// WM_CANCELMODE is received by the view, the View schedules the destruction of
// the render process. However in this case since there's no WebContents
// container, when the render process is destroyed, the RenderWidgetHost just
// deletes itself, which is safe because no one else should have any references
// to it (the WebContents does not).
//
// It should be noted that the RenderViewHost, not the RenderWidgetHost,
// handles IPC messages relating to the render process going away, since the
// way a RenderViewHost (WebContents) handles the process dying is different to
// the way a select popup does. As such the RenderWidgetHostView handles these
// messages for select popups. This placement is more out of convenience than
// anything else. When the view is live, these messages are forwarded to it by
// the RenderWidgetHost's IPC message map.
class CONTENT_EXPORT RenderWidgetHost : public IPC::Sender {
 public:
  // Returns the RenderWidgetHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderWidgetHost.
  static RenderWidgetHost* FromID(int32_t process_id, int32_t routing_id);

  // Returns an iterator to iterate over the global list of active render widget
  // hosts.
  static std::unique_ptr<RenderWidgetHostIterator> GetRenderWidgetHosts();

  ~RenderWidgetHost() override {}

  // Returns the viz::FrameSinkId that this object uses to put things on screen.
  // This value is constant throughout the lifetime of this object. Note that
  // until a RenderWidgetHostView is created, initialized, and assigned to this
  // object, viz may not be aware of this FrameSinkId.
  virtual const viz::FrameSinkId& GetFrameSinkId() = 0;

  // Update the text direction of the focused input element and notify it to a
  // renderer process.
  // These functions have two usage scenarios: changing the text direction
  // from a menu (as Safari does), and; changing the text direction when a user
  // presses a set of keys (as IE and Firefox do).
  // 1. Change the text direction from a menu.
  // In this scenario, we receive a menu event only once and we should update
  // the text direction immediately when a user chooses a menu item. So, we
  // should call both functions at once as listed in the following snippet.
  //   void RenderViewHost::SetTextDirection(WebTextDirection direction) {
  //     UpdateTextDirection(direction);
  //     NotifyTextDirection();
  //   }
  // 2. Change the text direction when pressing a set of keys.
  // Because of auto-repeat, we may receive the same key-press event many
  // times while we presses the keys and it is nonsense to send the same IPC
  // message every time when we receive a key-press event.
  // To suppress the number of IPC messages, we just update the text direction
  // when receiving a key-press event and send an IPC message when we release
  // the keys as listed in the following snippet.
  //   if (key_event.type == WebKeyboardEvent::KEY_DOWN) {
  //     if (key_event.windows_key_code == 'A' &&
  //         key_event.modifiers == WebKeyboardEvent::CTRL_KEY) {
  //       UpdateTextDirection(dir);
  //     } else {
  //       CancelUpdateTextDirection();
  //     }
  //   } else if (key_event.type == WebKeyboardEvent::KEY_UP) {
  //     NotifyTextDirection();
  //   }
  // Once we cancel updating the text direction, we have to ignore all
  // succeeding UpdateTextDirection() requests until calling
  // NotifyTextDirection(). (We may receive keydown events even after we
  // canceled updating the text direction because of auto-repeat.)
  // Note: we cannot undo this change for compatibility with Firefox and IE.
  virtual void UpdateTextDirection(blink::WebTextDirection direction) = 0;
  virtual void NotifyTextDirection() = 0;

  virtual void Focus() = 0;
  virtual void Blur() = 0;

  // Tests may need to flush IPCs to ensure deterministic behavior.
  virtual void FlushForTesting() = 0;

  // Sets whether the renderer should show controls in an active state.  On all
  // platforms except mac, that's the same as focused. On mac, the frontmost
  // window will show active controls even if the focus is not in the web
  // contents, but e.g. in the omnibox.
  virtual void SetActive(bool active) = 0;

  // Forwards the given message to the renderer. These are called by
  // the view when it has received a message.
  virtual void ForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) = 0;
  virtual void ForwardWheelEvent(
      const blink::WebMouseWheelEvent& wheel_event) = 0;
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) = 0;
  virtual void ForwardKeyboardEventWithLatencyInfo(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info) = 0;
  virtual void ForwardGestureEvent(
      const blink::WebGestureEvent& gesture_event) = 0;

  virtual RenderProcessHost* GetProcess() = 0;

  virtual int GetRoutingID() = 0;

  // Gets the View of this RenderWidgetHost. Can be nullptr, e.g. if the
  // RenderWidget is being destroyed or the render process crashed. You should
  // never cache this pointer since it can become nullptr if the renderer
  // crashes, instead you should always ask for it using the accessor.
  virtual RenderWidgetHostView* GetView() = 0;

  // Returns true if the renderer is considered unresponsive.
  virtual bool IsCurrentlyUnresponsive() = 0;

  // Called to propagate updated visual properties to the renderer. Returns
  // whether the renderer has been informed of updated properties.
  virtual bool SynchronizeVisualProperties() = 0;

  // Access to the implementation's IPC::Listener::OnMessageReceived. Intended
  // only for test code.

  // Add/remove a callback that can handle key presses without requiring focus.
  typedef base::Callback<bool(const NativeWebKeyboardEvent&)>
      KeyPressEventCallback;
  virtual void AddKeyPressEventCallback(
      const KeyPressEventCallback& callback) = 0;
  virtual void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) = 0;

  // Add/remove a callback that can handle all kinds of mouse events.
  typedef base::Callback<bool(const blink::WebMouseEvent&)> MouseEventCallback;
  virtual void AddMouseEventCallback(const MouseEventCallback& callback) = 0;
  virtual void RemoveMouseEventCallback(const MouseEventCallback& callback) = 0;

  // Observer for WebInputEvents.
  class InputEventObserver {
   public:
    virtual ~InputEventObserver() {}

    virtual void OnInputEvent(const blink::WebInputEvent&) {}
    virtual void OnInputEventAck(InputEventAckSource source,
                                 InputEventAckState state,
                                 const blink::WebInputEvent&) {}

#if defined(OS_ANDROID)
    // Not all key events are triggered through InputEvent on Android.
    // InputEvents are only triggered when user typed in through number bar on
    // Android keyboard. This function is triggered when text is committed in
    // input form.
    virtual void OnImeTextCommittedEvent(const base::string16& text_str) {}
    // This function is triggered when composing text is updated. Note that
    // text_str contains all text that is currently under composition rather
    // than updated text only.
    virtual void OnImeSetComposingTextEvent(const base::string16& text_str) {}
    // This function is triggered when composing text is filled into the input
    // form.
    virtual void OnImeFinishComposingTextEvent() {}
#endif
  };

  // Add/remove an input event observer.
  virtual void AddInputEventObserver(InputEventObserver* observer) = 0;
  virtual void RemoveInputEventObserver(InputEventObserver* observer) = 0;

#if defined(OS_ANDROID)
  // Add/remove an Ime input event observer.
  virtual void AddImeInputEventObserver(InputEventObserver* observer) = 0;
  virtual void RemoveImeInputEventObserver(InputEventObserver* observer) = 0;
#endif

  // Add and remove observers for widget host events. The order in which
  // notifications are sent to observers is undefined. Observers must be sure to
  // remove the observer before they go away.
  virtual void AddObserver(RenderWidgetHostObserver* observer) = 0;
  virtual void RemoveObserver(RenderWidgetHostObserver* observer) = 0;

  // Get the screen info corresponding to this render widget.
  virtual void GetScreenInfo(ScreenInfo* result) = 0;

  // Drag-and-drop drop target messages that get sent to Blink.
  virtual void DragTargetDragEnter(
      const DropData& drop_data,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) {}
  virtual void DragTargetDragEnterWithMetaData(
      const std::vector<DropData::Metadata>& metadata,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) {}
  virtual void DragTargetDragOver(
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) {}
  virtual void DragTargetDragLeave(const gfx::PointF& client_point,
                                   const gfx::PointF& screen_point) {}
  virtual void DragTargetDrop(const DropData& drop_data,
                              const gfx::PointF& client_pt,
                              const gfx::PointF& screen_pt,
                              int key_modifiers) {}

  // Notifies the renderer that a drag operation that it started has ended,
  // either in a drop or by being cancelled.
  virtual void DragSourceEndedAt(const gfx::PointF& client_pt,
                                 const gfx::PointF& screen_pt,
                                 blink::WebDragOperation operation) {}

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  virtual void DragSourceSystemDragEnded() {}

  // Filters drop data before it is passed to RenderWidgetHost.
  virtual void FilterDropData(DropData* drop_data) {}

  // Sets cursor to a specified one when it is over this widget.
  virtual void SetCursor(const CursorInfo& cursor_info) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_

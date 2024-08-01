// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/scoped_observation_traits.h"
#include "build/build_config.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sender.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/input/input_event_result.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen_infos.h"
#include "ui/surface/transport_dib.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace cc {
enum class TouchAction;
}

namespace display {
struct ScreenInfo;
}

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
class LatencyInfo;
}

namespace viz {
class FrameSinkId;
}

namespace content {
class RenderProcessHost;
class RenderWidgetHostIterator;
class RenderWidgetHostObserver;
class RenderWidgetHostView;

// A RenderWidgetHost acts as the abstraction for compositing and input
// functionality. It can exist in 3 different scenarios:
//
// 1. Popups, which are spawned in situations like <select> menus or
//    HTML calendar widgets. These are browser-implemented widgets that
//    are created and owned by WebContents in response to a renderer
//    request. Since they are divorced from the web page (they are not
//    clipped to the bounds of the page), they are an independent
//    compositing and input target. As they are owned by WebContents,
//    they are also destroyed by WebContents.
//
// 2. Main frames, which are a root frame of a WebContents. These frames
//    are separated from the browser UI for compositing and input, as the
//    renderer lives in its own coordinate space. These are attached to
//    the lifetime of the main frame (currently, owned by the
//    RenderViewHost, though that should change one day as per
//    https://crbug.com/419087).
//
// 3. Child local root frames, which are iframes isolated from their
//    parent frame for security or performance purposes. This allows
//    them to be placed in an arbitrary process relative to their
//    parent frame. Since they are isolated from the parent, they live
//    in their own coordinate space and are an independent unit of
//    compositing and input. These are attached to the lifetime of
//    the local root frame, and are explicitly owned by the
//    RenderFrameHost.
//
// A RenderWidgetHost is platform-agnostic. It defers platform-specific
// behaviour to its RenderWidgetHostView, which ties the compositing
// output into the native browser UI. Child local root frames also have
// a separate "platform" RenderWidgetHostView type at this time, though
// it stretches the abstraction uncomfortably.
//
// The RenderWidgetHostView has a complex and somewhat broken lifetime as
// of this writing (see, e.g. https://crbug.com/1161585). It is eagerly
// created along with the RenderWidgetHost on the first creation, before
// the renderer process may exist. It is destroyed if the renderer process
// exits, and not recreated at that time. Then it is recreated lazily when
// the associated renderer frame/widget is recreated.
class CONTENT_EXPORT RenderWidgetHost {
 public:
  // Returns the RenderWidgetHost given its ID and the ID of its render process.
  // Returns nullptr if the IDs do not correspond to a live RenderWidgetHost.
  static RenderWidgetHost* FromID(int32_t process_id, int32_t routing_id);

  // Returns an iterator over the global list of active RenderWidgetHosts.
  static std::unique_ptr<RenderWidgetHostIterator> GetRenderWidgetHosts();

  virtual ~RenderWidgetHost() {}

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
  //   void RenderViewHost::SetTextDirection(
  //       base::i18n::TextDirection direction) {
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
  virtual void UpdateTextDirection(base::i18n::TextDirection direction) = 0;
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
      const input::NativeWebKeyboardEvent& key_event) = 0;
  virtual void ForwardKeyboardEventWithLatencyInfo(
      const input::NativeWebKeyboardEvent& key_event,
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
  // true if visual properties have changed since last call.
  virtual bool SynchronizeVisualProperties() = 0;

  // Access to the implementation's IPC::Listener::OnMessageReceived. Intended
  // only for test code.

  // Add/remove a callback that can handle key presses without requiring focus.
  using KeyPressEventCallback =
      base::RepeatingCallback<bool(const input::NativeWebKeyboardEvent&)>;
  virtual void AddKeyPressEventCallback(
      const KeyPressEventCallback& callback) = 0;
  virtual void RemoveKeyPressEventCallback(
      const KeyPressEventCallback& callback) = 0;

  // Add/remove a callback that can handle all kinds of mouse events.
  using MouseEventCallback =
      base::RepeatingCallback<bool(const blink::WebMouseEvent&)>;
  virtual void AddMouseEventCallback(const MouseEventCallback& callback) = 0;
  virtual void RemoveMouseEventCallback(const MouseEventCallback& callback) = 0;

  // Adds a callback that, when it returns true, will suppress IME
  // display.
  using SuppressShowingImeCallback = base::RepeatingCallback<bool()>;
  virtual void AddSuppressShowingImeCallback(
      const SuppressShowingImeCallback& callback) = 0;
  // Removes the callback to suppress IME display. If `trigger_ime` is set tu
  // true, it will also try to show the IME display after the callback removal.
  virtual void RemoveSuppressShowingImeCallback(
      const SuppressShowingImeCallback& callback,
      bool trigger_ime) = 0;

  // Observer for WebInputEvents.
  class InputEventObserver {
   public:
    virtual ~InputEventObserver() {}

    virtual void OnInputEvent(const blink::WebInputEvent&) {}
    virtual void OnInputEventAck(blink::mojom::InputEventResultSource source,
                                 blink::mojom::InputEventResultState state,
                                 const blink::WebInputEvent&) {}

#if BUILDFLAG(IS_ANDROID)
    // Not all key events are triggered through InputEvent on Android.
    // InputEvents are only triggered when user typed in through number bar on
    // Android keyboard. This function is triggered when text is committed in
    // input form.
    virtual void OnImeTextCommittedEvent(const std::u16string& text_str) {}
    // This function is triggered when composing text is updated. Note that
    // text_str contains all text that is currently under composition rather
    // than updated text only.
    virtual void OnImeSetComposingTextEvent(const std::u16string& text_str) {}
    // This function is triggered when composing text is filled into the input
    // form.
    virtual void OnImeFinishComposingTextEvent() {}
#endif
  };

  // Add/remove an input event observer.
  virtual void AddInputEventObserver(InputEventObserver* observer) = 0;
  virtual void RemoveInputEventObserver(InputEventObserver* observer) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Add/remove an Ime input event observer.
  virtual void AddImeInputEventObserver(InputEventObserver* observer) = 0;
  virtual void RemoveImeInputEventObserver(InputEventObserver* observer) = 0;
#endif

  // Add and remove observers for widget host events. The order in which
  // notifications are sent to observers is undefined. Observers must be sure to
  // remove the observer before they go away.
  virtual void AddObserver(RenderWidgetHostObserver* observer) = 0;
  virtual void RemoveObserver(RenderWidgetHostObserver* observer) = 0;

  // Get info regarding the screen showing this RenderWidgetHost.
  virtual display::ScreenInfo GetScreenInfo() const = 0;

  // Get info regarding all screens, including which screen is currently showing
  // this RenderWidgetHost.
  virtual display::ScreenInfos GetScreenInfos() const = 0;

  // This must always return the same device scale factor as GetScreenInfo.
  virtual float GetDeviceScaleFactor() = 0;

  // Get the allowed touch action corresponding to this RenderWidgetHost.
  virtual std::optional<cc::TouchAction> GetAllowedTouchAction() = 0;

  // Write a representation of this object into a trace.
  virtual void WriteIntoTrace(perfetto::TracedValue context) = 0;

  using DragOperationCallback =
      base::OnceCallback<void(::ui::mojom::DragOperation, bool)>;
  // Drag-and-drop drop target messages that get sent to Blink.
  virtual void DragTargetDragEnter(const DropData& drop_data,
                                   const gfx::PointF& client_pt,
                                   const gfx::PointF& screen_pt,
                                   blink::DragOperationsMask operations_allowed,
                                   int key_modifiers,
                                   DragOperationCallback callback) {}
  virtual void DragTargetDragEnterWithMetaData(
      const std::vector<DropData::Metadata>& metadata,
      const gfx::PointF& client_pt,
      const gfx::PointF& screen_pt,
      blink::DragOperationsMask operations_allowed,
      int key_modifiers,
      DragOperationCallback callback) {}
  virtual void DragTargetDragOver(const gfx::PointF& client_pt,
                                  const gfx::PointF& screen_pt,
                                  blink::DragOperationsMask operations_allowed,
                                  int key_modifiers,
                                  DragOperationCallback callback) {}
  virtual void DragTargetDragLeave(const gfx::PointF& client_point,
                                   const gfx::PointF& screen_point) {}
  virtual void DragTargetDrop(const DropData& drop_data,
                              const gfx::PointF& client_pt,
                              const gfx::PointF& screen_pt,
                              int key_modifiers,
                              base::OnceClosure callback) {}

  // Notifies the renderer that a drag operation that it started has ended,
  // either in a drop or by being cancelled.
  virtual void DragSourceEndedAt(const gfx::PointF& client_pt,
                                 const gfx::PointF& screen_pt,
                                 ui::mojom::DragOperation operation,
                                 base::OnceClosure callback) {}

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  virtual void DragSourceSystemDragEnded() {}

  // Filters drop data before it is passed to RenderWidgetHost.
  virtual void FilterDropData(DropData* drop_data) {}

  // Sets cursor to a specified one when it is over this widget.
  virtual void SetCursor(const ui::Cursor& cursor) {}

  // Shows the context menu using the specified point as anchor point.
  virtual void ShowContextMenuAtPoint(const gfx::Point& point,
                                      const ui::MenuSourceType source_type) {}

  // Roundtrips through the renderer and compositor pipeline to ensure that any
  // changes to the contents resulting from operations executed prior to this
  // call are visible on screen. The call completes asynchronously (if it
  // succeeds) by running the supplied |callback| with a value of true upon
  // successful completion and false otherwise when the widget is destroyed.
  // This can run synchronously on failure.
  using VisualStateCallback = base::OnceCallback<void(bool)>;
  virtual void InsertVisualStateCallback(VisualStateCallback callback) {}
};

}  // namespace content

namespace base {
template <>
struct ScopedObservationTraits<content::RenderWidgetHost,
                               content::RenderWidgetHost::InputEventObserver> {
  static void AddObserver(
      content::RenderWidgetHost* rwh,
      content::RenderWidgetHost::InputEventObserver* observer) {
    rwh->AddInputEventObserver(observer);
  }
  static void RemoveObserver(
      content::RenderWidgetHost* rwh,
      content::RenderWidgetHost::InputEventObserver* observer) {
    rwh->RemoveInputEventObserver(observer);
  }
};
}  // namespace base

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_WIDGET_HOST_H_

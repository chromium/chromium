// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_fullscreen_pepper.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "cc/paint/paint_canvas.h"
#include "content/common/view_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/renderer/compositor/layer_tree_view.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_cursor_info.h"
#include "third_party/blink/public/platform/web_gesture_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gpu_preference.h"

using blink::WebCoalescedInputEvent;
using blink::WebImeTextSpan;
using blink::WebCursorInfo;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebTextDirection;
using blink::WebTextInputType;
using blink::WebVector;
using blink::WebWidget;

namespace content {

namespace {

class FullscreenMouseLockDispatcher : public MouseLockDispatcher {
 public:
  explicit FullscreenMouseLockDispatcher(RenderWidgetFullscreenPepper* widget);
  ~FullscreenMouseLockDispatcher() override;

 private:
  // MouseLockDispatcher implementation.
  void SendLockMouseRequest(blink::WebLocalFrame* requester_frame,
                            bool request_unadjusted_movement) override;
  void SendUnlockMouseRequest() override;

  RenderWidgetFullscreenPepper* widget_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMouseLockDispatcher);
};

WebMouseEvent WebMouseEventFromGestureEvent(const WebGestureEvent& gesture) {

  // Only convert touch screen gesture events, do not convert
  // touchpad/mouse wheel gesture events. (crbug.com/620974)
  if (gesture.SourceDevice() != blink::WebGestureDevice::kTouchscreen)
    return WebMouseEvent();

  WebInputEvent::Type type = WebInputEvent::kUndefined;
  switch (gesture.GetType()) {
    case WebInputEvent::kGestureScrollBegin:
      type = WebInputEvent::kMouseDown;
      break;
    case WebInputEvent::kGestureScrollUpdate:
      type = WebInputEvent::kMouseMove;
      break;
    case WebInputEvent::kGestureFlingStart:
      // A scroll gesture on the touchscreen may end with a GestureScrollEnd
      // when there is no velocity, or a GestureFlingStart when it has a
      // velocity. In both cases, it should end the drag that was initiated by
      // the GestureScrollBegin (and subsequent GestureScrollUpdate) events.
      type = WebInputEvent::kMouseUp;
      break;
    case WebInputEvent::kGestureScrollEnd:
      type = WebInputEvent::kMouseUp;
      break;
    default:
      return WebMouseEvent();
  }

  WebMouseEvent mouse(type,
                      gesture.GetModifiers() | WebInputEvent::kLeftButtonDown,
                      gesture.TimeStamp());
  mouse.button = WebMouseEvent::Button::kLeft;
  mouse.click_count = (mouse.GetType() == WebInputEvent::kMouseDown ||
                       mouse.GetType() == WebInputEvent::kMouseUp);

  mouse.SetPositionInWidget(gesture.PositionInWidget().x,
                            gesture.PositionInWidget().y);
  mouse.SetPositionInScreen(gesture.PositionInScreen().x,
                            gesture.PositionInScreen().y);

  return mouse;
}

FullscreenMouseLockDispatcher::FullscreenMouseLockDispatcher(
    RenderWidgetFullscreenPepper* widget) : widget_(widget) {
}

FullscreenMouseLockDispatcher::~FullscreenMouseLockDispatcher() {
}

void FullscreenMouseLockDispatcher::SendLockMouseRequest(
    blink::WebLocalFrame* requester_frame,
    bool request_unadjusted_movement) {
  // TODO(mustaq): Why is it not checking user activation state at all?  In
  // particular, the last Boolean param ("privileged") in the IPC below looks
  // scary without this check.
  widget_->Send(new WidgetHostMsg_LockMouse(widget_->routing_id(), false, true,
                                            request_unadjusted_movement));
}

void FullscreenMouseLockDispatcher::SendUnlockMouseRequest() {
  widget_->Send(new WidgetHostMsg_UnlockMouse(widget_->routing_id()));
}

// WebWidget that simply wraps the pepper plugin.
// TODO(piman): figure out IME and implement setComposition and friends if
// necessary.
class PepperWidget : public WebWidget {
 public:
  explicit PepperWidget(RenderWidgetFullscreenPepper* widget,
                        const blink::WebURL& local_main_frame_url)
      : widget_(widget), local_main_frame_url_(local_main_frame_url) {}

  virtual ~PepperWidget() {}

  // WebWidget API
  void SetAnimationHost(cc::AnimationHost*) override {
    // Does nothing, as the LayerTreeView can be accessed from the RenderWidget
    // directly.
  }

  void Close() override { delete this; }

  WebSize Size() override { return size_; }

  bool IsPepperWidget() const override { return true; }

  void Resize(const WebSize& size) override {
    if (!widget_->plugin() || size_ == size)
      return;

    size_ = size;
    WebRect plugin_rect(0, 0, size_.width, size_.height);
    widget_->plugin()->ViewChanged(plugin_rect, plugin_rect, plugin_rect);
  }

  void ThemeChanged() override { NOTIMPLEMENTED(); }

  blink::WebHitTestResult HitTestResultAt(const gfx::Point&) override {
    NOTIMPLEMENTED();
    return {};
  }

  WebInputEventResult HandleInputEvent(
      const WebCoalescedInputEvent& coalesced_event) override {
    if (!widget_->plugin())
      return WebInputEventResult::kNotHandled;

    const WebInputEvent& event = coalesced_event.Event();

    // This cursor info is ignored, we always set the cursor directly from
    // RenderWidgetFullscreenPepper::DidChangeCursor.
    WebCursorInfo cursor;

    // Pepper plugins do not accept gesture events. So do not send the gesture
    // events directly to the plugin. Instead, try to convert them to equivalent
    // mouse events, and then send to the plugin.
    if (WebInputEvent::IsGestureEventType(event.GetType())) {
      bool result = false;
      const WebGestureEvent* gesture_event =
          static_cast<const WebGestureEvent*>(&event);
      switch (event.GetType()) {
        case WebInputEvent::kGestureTap: {
          WebMouseEvent mouse(WebInputEvent::kMouseMove,
                              gesture_event->GetModifiers(),
                              gesture_event->TimeStamp());
          mouse.SetPositionInWidget(gesture_event->PositionInWidget().x,
                                    gesture_event->PositionInWidget().y);
          mouse.SetPositionInScreen(gesture_event->PositionInScreen().x,
                                    gesture_event->PositionInScreen().y);
          mouse.movement_x = 0;
          mouse.movement_y = 0;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.SetType(WebInputEvent::kMouseDown);
          mouse.button = WebMouseEvent::Button::kLeft;
          mouse.click_count = gesture_event->data.tap.tap_count;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.SetType(WebInputEvent::kMouseUp);
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }

        default: {
          WebMouseEvent mouse = WebMouseEventFromGestureEvent(*gesture_event);
          if (mouse.GetType() != WebInputEvent::kUndefined)
            result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }
      }
      return result ? WebInputEventResult::kHandledApplication
                    : WebInputEventResult::kNotHandled;
    }

    bool result = widget_->plugin()->HandleInputEvent(event, &cursor);

    // For normal web pages, WebViewImpl does input event translations and
    // generates context menu events. Since we don't have a WebView, we need to
    // do the necessary translation ourselves.
    if (WebInputEvent::IsMouseEventType(event.GetType())) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      bool send_context_menu_event = false;
      // On Mac/Linux, we handle it on mouse down.
      // On Windows, we handle it on mouse up.
#if defined(OS_WIN)
      send_context_menu_event =
          mouse_event.GetType() == WebInputEvent::kMouseUp &&
          mouse_event.button == WebMouseEvent::Button::kRight;
#elif defined(OS_MACOSX)
      send_context_menu_event =
          mouse_event.GetType() == WebInputEvent::kMouseDown &&
          (mouse_event.button == WebMouseEvent::Button::kRight ||
           (mouse_event.button == WebMouseEvent::Button::kLeft &&
            mouse_event.GetModifiers() & WebMouseEvent::kControlKey));
#else
      send_context_menu_event =
          mouse_event.GetType() == WebInputEvent::kMouseDown &&
          mouse_event.button == WebMouseEvent::Button::kRight;
#endif
      if (send_context_menu_event) {
        WebMouseEvent context_menu_event(mouse_event);
        context_menu_event.SetType(WebInputEvent::kContextMenu);
        widget_->plugin()->HandleInputEvent(context_menu_event, &cursor);
      }
    }
    return result ? WebInputEventResult::kHandledApplication
                  : WebInputEventResult::kNotHandled;
  }

  blink::WebURL GetURLForDebugTrace() override { return local_main_frame_url_; }

 private:
  RenderWidgetFullscreenPepper* widget_;
  WebSize size_;
  blink::WebURL local_main_frame_url_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};

}  // anonymous namespace

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    int32_t routing_id,
    RenderWidget::ShowCallback show_callback,
    CompositorDependencies* compositor_deps,
    const ScreenInfo& screen_info,
    PepperPluginInstanceImpl* plugin,
    const blink::WebURL& local_main_frame_url,
    mojo::PendingReceiver<mojom::Widget> widget_receiver) {
  DCHECK_NE(MSG_ROUTING_NONE, routing_id);
  DCHECK(show_callback);
  RenderWidgetFullscreenPepper* widget = new RenderWidgetFullscreenPepper(
      routing_id, compositor_deps, plugin, std::move(widget_receiver));
  widget->InitForPepperFullscreen(
      std::move(show_callback), new PepperWidget(widget, local_main_frame_url),
      screen_info);
  return widget;
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    PepperPluginInstanceImpl* plugin,
    mojo::PendingReceiver<mojom::Widget> widget_receiver)
    : RenderWidget(routing_id,
                   compositor_deps,
                   /*display_mode=*/blink::mojom::DisplayMode::kUndefined,
                   /*is_undead=*/false,
                   /*hidden=*/false,
                   /*never_visible=*/false,
                   std::move(widget_receiver)),
      plugin_(plugin),
      mouse_lock_dispatcher_(
          std::make_unique<FullscreenMouseLockDispatcher>(this)) {}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
}

void RenderWidgetFullscreenPepper::ScrollRect(
    int dx, int dy, const blink::WebRect& rect) {
}

void RenderWidgetFullscreenPepper::Destroy() {
  // The plugin instance is going away reset any lock target that is set
  // on the dispatcher since this object can still live and receive IPC
  // responses and may call a dangling lock_target.
  mouse_lock_dispatcher_->ClearLockTarget();

  // This function is called by the plugin instance as it's going away, so reset
  // plugin_ to NULL to avoid calling into a dangling pointer e.g. on Close().
  plugin_ = nullptr;

  // After calling Destroy(), the plugin instance assumes that the layer is not
  // used by us anymore, so it may destroy the layer before this object goes
  // away.
  SetLayer(nullptr);

  // This instructs the browser process, which owns this object, to send back a
  // WidgetMsg_Close to destroy this object.
  Send(new WidgetHostMsg_Close(routing_id()));
}

void RenderWidgetFullscreenPepper::PepperDidChangeCursor(
    const blink::WebCursorInfo& cursor) {
  DidChangeCursor(cursor);
}

// TODO(danakj): These should be a scoped_refptr<cc::Layer>.
void RenderWidgetFullscreenPepper::SetLayer(cc::Layer* layer) {
  layer_ = layer;
  if (!layer_) {
    RenderWidget::SetRootLayer(nullptr);
    return;
  }
  UpdateLayerBounds();
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  layer_tree_host()->SetNonBlinkManagedRootLayer(layer_);
}

bool RenderWidgetFullscreenPepper::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetFullscreenPepper, msg)
    IPC_MESSAGE_FORWARD(WidgetMsg_LockMouse_ACK, mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnLockMouseACK)
    IPC_MESSAGE_FORWARD(WidgetMsg_MouseLockLost, mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  return RenderWidget::OnMessageReceived(msg);
}

void RenderWidgetFullscreenPepper::DidInitiatePaint() {
  if (plugin_)
    plugin_->ViewInitiatedPaint();
}

void RenderWidgetFullscreenPepper::Close(std::unique_ptr<RenderWidget> widget) {
  // If the fullscreen window is closed (e.g. user pressed escape), reset to
  // normal mode.
  if (plugin_)
    plugin_->FlashSetFullscreen(false, false);

  // Call Close on the base class to destroy the WebWidget instance.
  RenderWidget::Close(std::move(widget));
}

void RenderWidgetFullscreenPepper::AfterUpdateVisualProperties() {
  UpdateLayerBounds();
}

void RenderWidgetFullscreenPepper::UpdateLayerBounds() {
  if (!layer_)
    return;

  // The |layer_| is sized here to cover the entire renderer's compositor
  // viewport.
  gfx::Size layer_size = gfx::Rect(ViewRect()).size();
  // When IsUseZoomForDSFEnabled() is true, layout and compositor layer sizes
  // given by blink are all in physical pixels, and the compositor does not do
  // any scaling. But the ViewRect() is always in DIP so we must scale the layer
  // here as the compositor won't.
  if (compositor_deps()->IsUseZoomForDSFEnabled()) {
    layer_size = gfx::ScaleToCeiledSize(
        layer_size, GetOriginalScreenInfo().device_scale_factor);
  }
  layer_->SetBounds(layer_size);
}

}  // namespace content

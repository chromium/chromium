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
#include "content/renderer/gpu/layer_tree_view.h"
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
  void SendLockMouseRequest() override;
  void SendUnlockMouseRequest() override;

  RenderWidgetFullscreenPepper* widget_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMouseLockDispatcher);
};

WebMouseEvent WebMouseEventFromGestureEvent(const WebGestureEvent& gesture) {

  // Only convert touch screen gesture events, do not convert
  // touchpad/mouse wheel gesture events. (crbug.com/620974)
  if (gesture.SourceDevice() != blink::kWebGestureDeviceTouchscreen)
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

void FullscreenMouseLockDispatcher::SendLockMouseRequest() {
  widget_->Send(
      new WidgetHostMsg_LockMouse(widget_->routing_id(), false, true));
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
  void SetLayerTreeView(blink::WebLayerTreeView*) override {
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
    widget_->Invalidate();
  }

  void ThemeChanged() override { NOTIMPLEMENTED(); }

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
    PepperPluginInstanceImpl* plugin,
    const blink::WebURL& local_main_frame_url,
    const ScreenInfo& screen_info,
    mojom::WidgetRequest widget_request) {
  DCHECK_NE(MSG_ROUTING_NONE, routing_id);
  DCHECK(show_callback);
  scoped_refptr<RenderWidgetFullscreenPepper> widget(
      new RenderWidgetFullscreenPepper(routing_id, compositor_deps, plugin,
                                       screen_info, std::move(widget_request)));
  widget->Init(std::move(show_callback),
               new PepperWidget(widget.get(), local_main_frame_url));
  widget->AddRef();
  return widget.get();
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    PepperPluginInstanceImpl* plugin,
    const ScreenInfo& screen_info,
    mojom::WidgetRequest widget_request)
    : RenderWidget(routing_id,
                   compositor_deps,
                   WidgetType::kFrame,
                   screen_info,
                   blink::kWebDisplayModeUndefined,
                   false,
                   false,
                   false,
                   std::move(widget_request)),
      plugin_(plugin),
      layer_(nullptr),
      mouse_lock_dispatcher_(new FullscreenMouseLockDispatcher(this)) {}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
}

void RenderWidgetFullscreenPepper::Invalidate() {
  InvalidateRect(gfx::Rect(size()));
}

void RenderWidgetFullscreenPepper::InvalidateRect(const blink::WebRect& rect) {
  DidInvalidateRect(rect);
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

  Send(new WidgetHostMsg_Close(routing_id()));
  Release();
}

void RenderWidgetFullscreenPepper::PepperDidChangeCursor(
    const blink::WebCursorInfo& cursor) {
  DidChangeCursor(cursor);
}

// TODO(danakj): These should be a scoped_refptr<cc::Layer>.
void RenderWidgetFullscreenPepper::SetLayer(cc::Layer* layer) {
  layer_ = layer;
  if (!layer_) {
    if (layer_tree_view())
      layer_tree_view()->ClearRootLayer();
    return;
  }
  UpdateLayerBounds();
  layer_->SetIsDrawable(true);
  layer_tree_view()->SetRootLayer(layer_);
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

void RenderWidgetFullscreenPepper::Close() {
  // If the fullscreen window is closed (e.g. user pressed escape), reset to
  // normal mode.
  if (plugin_)
    plugin_->FlashSetFullscreen(false, false);

  // Call Close on the base class to destroy the WebWidget instance.
  RenderWidget::Close();
}

void RenderWidgetFullscreenPepper::OnSynchronizeVisualProperties(
    const VisualProperties& visual_properties) {
  RenderWidget::OnSynchronizeVisualProperties(visual_properties);
  UpdateLayerBounds();
}

void RenderWidgetFullscreenPepper::UpdateLayerBounds() {
  if (!layer_)
    return;

  if (compositor_deps()->IsUseZoomForDSFEnabled()) {
    // Note that root cc::Layers' bounds are specified in pixels (in contrast
    // with non-root cc::Layers' bounds, which are specified in DIPs).
    layer_->SetBounds(blink::WebSize(compositor_viewport_pixel_size()));
  } else {
    // For reasons that are unclear, the above comment doesn't appear to apply
    // when zoom for DSF is not enabled.
    // https://crbug.com/822252
    gfx::Size dip_size =
        gfx::ConvertSizeToDIP(GetOriginalScreenInfo().device_scale_factor,
                              compositor_viewport_pixel_size());
    layer_->SetBounds(blink::WebSize(dip_size));
  }
}

}  // namespace content

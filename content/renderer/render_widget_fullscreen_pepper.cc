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
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gl/gpu_preference.h"

using blink::WebCoalescedInputEvent;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebInputEventResult;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebRect;
using blink::WebSize;
using blink::WebString;
using blink::WebTextInputType;
using blink::WebVector;

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

  RenderWidgetFullscreenPepper* widget_;

  base::WeakPtrFactory<FullscreenMouseLockDispatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FullscreenMouseLockDispatcher);
};

WebMouseEvent WebMouseEventFromGestureEvent(const WebGestureEvent& gesture) {
  // Only convert touch screen gesture events, do not convert
  // touchpad/mouse wheel gesture events. (crbug.com/620974)
  if (gesture.SourceDevice() != blink::WebGestureDevice::kTouchscreen)
    return WebMouseEvent();

  WebInputEvent::Type type = WebInputEvent::Type::kUndefined;
  switch (gesture.GetType()) {
    case WebInputEvent::Type::kGestureScrollBegin:
      type = WebInputEvent::Type::kMouseDown;
      break;
    case WebInputEvent::Type::kGestureScrollUpdate:
      type = WebInputEvent::Type::kMouseMove;
      break;
    case WebInputEvent::Type::kGestureFlingStart:
      // A scroll gesture on the touchscreen may end with a GestureScrollEnd
      // when there is no velocity, or a GestureFlingStart when it has a
      // velocity. In both cases, it should end the drag that was initiated by
      // the GestureScrollBegin (and subsequent GestureScrollUpdate) events.
      type = WebInputEvent::Type::kMouseUp;
      break;
    case WebInputEvent::Type::kGestureScrollEnd:
      type = WebInputEvent::Type::kMouseUp;
      break;
    default:
      return WebMouseEvent();
  }

  WebMouseEvent mouse(type,
                      gesture.GetModifiers() | WebInputEvent::kLeftButtonDown,
                      gesture.TimeStamp());
  mouse.button = WebMouseEvent::Button::kLeft;
  mouse.click_count = (mouse.GetType() == WebInputEvent::Type::kMouseDown ||
                       mouse.GetType() == WebInputEvent::Type::kMouseUp);

  mouse.SetPositionInWidget(gesture.PositionInWidget());
  mouse.SetPositionInScreen(gesture.PositionInScreen());

  return mouse;
}

FullscreenMouseLockDispatcher::FullscreenMouseLockDispatcher(
    RenderWidgetFullscreenPepper* widget)
    : widget_(widget) {}

FullscreenMouseLockDispatcher::~FullscreenMouseLockDispatcher() = default;

void FullscreenMouseLockDispatcher::SendLockMouseRequest(
    blink::WebLocalFrame* requester_frame,
    bool request_unadjusted_movement) {
  bool has_transient_user_activation =
      requester_frame ? requester_frame->HasTransientUserActivation() : false;

  widget_->GetWebWidget()->RequestMouseLock(
      has_transient_user_activation, /*privileged=*/true,
      request_unadjusted_movement,
      base::BindOnce(&MouseLockDispatcher::OnLockMouseACK,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // anonymous namespace

// We place the WebExternalWidgetClient interface on a separate class because
// RenderWidget implements blink::WebWidgetClient, which is not used for
// WebExternalWidgets, but may have similar method definitions as this
// interface.
class PepperExternalWidgetClient : public blink::WebExternalWidgetClient {
 public:
  explicit PepperExternalWidgetClient(RenderWidgetFullscreenPepper* widget)
      : widget_(widget) {}
  ~PepperExternalWidgetClient() override = default;

  // blink::WebExternalWidgetClient overrides:
  blink::WebInputEventResult HandleInputEvent(
      const blink::WebCoalescedInputEvent& event) override {
    return widget_->ProcessInputEvent(event);
  }

  blink::WebInputEventResult DispatchBufferedTouchEvents() override {
    return WebInputEventResult::kNotHandled;
  }

  void DidResize(const gfx::Size& size) override { widget_->DidResize(size); }

  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override {
    widget_->RequestNewLayerTreeFrameSink(std::move(callback));
  }

  void RecordTimeToFirstActivePaint(base::TimeDelta duration) override {
    widget_->RecordTimeToFirstActivePaint(duration);
  }

  void DidCommitAndDrawCompositorFrame() override {
    widget_->DidInitiatePaint();
  }

  void DidUpdateVisualProperties() override { widget_->UpdateLayerBounds(); }

 private:
  RenderWidgetFullscreenPepper* widget_;
};

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    AgentSchedulingGroup& agent_scheduling_group,
    int32_t routing_id,
    RenderWidget::ShowCallback show_callback,
    CompositorDependencies* compositor_deps,
    const blink::ScreenInfo& screen_info,
    PepperPluginInstanceImpl* plugin,
    const blink::WebURL& local_main_frame_url,
    mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> blink_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> blink_widget) {
  DCHECK_NE(MSG_ROUTING_NONE, routing_id);
  DCHECK(show_callback);
  RenderWidgetFullscreenPepper* render_widget =
      new RenderWidgetFullscreenPepper(
          agent_scheduling_group, routing_id, compositor_deps, plugin,
          std::move(blink_widget_host), std::move(blink_widget),
          local_main_frame_url);
  render_widget->InitForPepperFullscreen(std::move(show_callback),
                                         render_widget->blink_widget_.get(),
                                         screen_info);
  return render_widget;
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    AgentSchedulingGroup& agent_scheduling_group,
    int32_t routing_id,
    CompositorDependencies* compositor_deps,
    PepperPluginInstanceImpl* plugin,
    mojo::PendingAssociatedRemote<blink::mojom::WidgetHost> mojo_widget_host,
    mojo::PendingAssociatedReceiver<blink::mojom::Widget> mojo_widget,
    blink::WebURL main_frame_url)
    : RenderWidget(agent_scheduling_group, routing_id, compositor_deps),
      plugin_(plugin),
      mouse_lock_dispatcher_(
          std::make_unique<FullscreenMouseLockDispatcher>(this)),
      widget_client_(std::make_unique<PepperExternalWidgetClient>(this)) {
  blink_widget_ = blink::WebExternalWidget::Create(
      widget_client_.get(), main_frame_url, std::move(mojo_widget_host),
      std::move(mojo_widget));
}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() = default;

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
    const ui::Cursor& cursor) {
  blink_widget_->SetCursor(cursor);
}

void RenderWidgetFullscreenPepper::SetLayer(scoped_refptr<cc::Layer> layer) {
  layer_ = layer.get();
  if (!layer_) {
    blink_widget_->SetRootLayer(nullptr);
    return;
  }
  UpdateLayerBounds();
  layer_->SetIsDrawable(true);
  layer_->SetHitTestable(true);
  blink_widget_->SetRootLayer(std::move(layer));
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

void RenderWidgetFullscreenPepper::UpdateLayerBounds() {
  if (!layer_)
    return;

  // The |layer_| is sized here to cover the entire renderer's compositor
  // viewport.
  gfx::Size layer_size = gfx::Rect(GetWebWidget()->ViewRect()).size();
  // When IsUseZoomForDSFEnabled() is true, layout and compositor layer sizes
  // given by blink are all in physical pixels, and the compositor does not do
  // any scaling. But the ViewRect() is always in DIP so we must scale the layer
  // here as the compositor won't.
  if (compositor_deps()->IsUseZoomForDSFEnabled()) {
    layer_size = gfx::ScaleToCeiledSize(
        layer_size,
        GetWebWidget()->GetOriginalScreenInfo().device_scale_factor);
  }
  layer_->SetBounds(layer_size);
}

WebInputEventResult RenderWidgetFullscreenPepper::ProcessInputEvent(
    const WebCoalescedInputEvent& coalesced_event) {
  if (!plugin())
    return WebInputEventResult::kNotHandled;

  const WebInputEvent& event = coalesced_event.Event();

  // This cursor is ignored, we always set the cursor directly from
  // RenderWidgetFullscreenPepper::DidChangeCursor.
  ui::Cursor cursor;

  // Pepper plugins do not accept gesture events. So do not send the gesture
  // events directly to the plugin. Instead, try to convert them to equivalent
  // mouse events, and then send to the plugin.
  if (blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    bool result = false;
    const WebGestureEvent* gesture_event =
        static_cast<const WebGestureEvent*>(&event);
    switch (event.GetType()) {
      case WebInputEvent::Type::kGestureTap: {
        WebMouseEvent mouse(WebInputEvent::Type::kMouseMove,
                            gesture_event->GetModifiers(),
                            gesture_event->TimeStamp());
        mouse.SetPositionInWidget(gesture_event->PositionInWidget());
        mouse.SetPositionInScreen(gesture_event->PositionInScreen());
        mouse.movement_x = 0;
        mouse.movement_y = 0;
        result |= plugin()->HandleInputEvent(mouse, &cursor);

        mouse.SetType(WebInputEvent::Type::kMouseDown);
        mouse.button = WebMouseEvent::Button::kLeft;
        mouse.click_count = gesture_event->data.tap.tap_count;
        result |= plugin()->HandleInputEvent(mouse, &cursor);

        mouse.SetType(WebInputEvent::Type::kMouseUp);
        result |= plugin()->HandleInputEvent(mouse, &cursor);
        break;
      }

      default: {
        WebMouseEvent mouse = WebMouseEventFromGestureEvent(*gesture_event);
        if (mouse.GetType() != WebInputEvent::Type::kUndefined)
          result |= plugin()->HandleInputEvent(mouse, &cursor);
        break;
      }
    }
    return result ? WebInputEventResult::kHandledApplication
                  : WebInputEventResult::kNotHandled;
  }

  bool result = plugin()->HandleInputEvent(event, &cursor);

  // For normal web pages, WebViewImpl does input event translations and
  // generates context menu events. Since we don't have a WebView, we need to
  // do the necessary translation ourselves.
  if (WebInputEvent::IsMouseEventType(event.GetType())) {
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    bool send_context_menu_event = false;
    // On Mac/Linux, we handle it on mouse down.
    // On Windows, we handle it on mouse up.
#if defined(OS_WIN)
    send_context_menu_event =
        mouse_event.GetType() == WebInputEvent::Type::kMouseUp &&
        mouse_event.button == WebMouseEvent::Button::kRight;
#elif defined(OS_MAC)
    send_context_menu_event =
        mouse_event.GetType() == WebInputEvent::Type::kMouseDown &&
        (mouse_event.button == WebMouseEvent::Button::kRight ||
         (mouse_event.button == WebMouseEvent::Button::kLeft &&
          mouse_event.GetModifiers() & WebMouseEvent::kControlKey));
#else
    send_context_menu_event =
        mouse_event.GetType() == WebInputEvent::Type::kMouseDown &&
        mouse_event.button == WebMouseEvent::Button::kRight;
#endif
    if (send_context_menu_event) {
      WebMouseEvent context_menu_event(mouse_event);
      context_menu_event.SetType(WebInputEvent::Type::kContextMenu);
      plugin()->HandleInputEvent(context_menu_event, &cursor);
    }
  }
  return result ? WebInputEventResult::kHandledApplication
                : WebInputEventResult::kNotHandled;
}

void RenderWidgetFullscreenPepper::DidResize(const gfx::Size& size) {
  if (!plugin())
    return;
  gfx::Rect plugin_rect(size);
  plugin()->ViewChanged(plugin_rect, plugin_rect, plugin_rect);
}

}  // namespace content

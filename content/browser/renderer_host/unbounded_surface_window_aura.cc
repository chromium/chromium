// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window_aura.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_surface.h"
#include "ui/compositor/layer_textured.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace content {

namespace {

gfx::Rect ConvertRectFromScreen(aura::Window* window,
                                const gfx::Rect& bounds_in_screen) {
  if (!window || !window->parent()) {
    return bounds_in_screen;
  }
  gfx::Point relative_origin = bounds_in_screen.origin();
  aura::Window* root = window->GetRootWindow();
  if (root) {
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root);
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(window->parent(),
                                                     &relative_origin);
    }
  }
  return gfx::Rect(relative_origin, bounds_in_screen.size());
}

// Returns true for modifier keys, which do not dismiss the surface on their
// own so that a shortcut aimed at the surface can still be typed.
bool IsModifierKey(ui::KeyboardCode key_code) {
  switch (key_code) {
    case ui::VKEY_SHIFT:
    case ui::VKEY_LSHIFT:
    case ui::VKEY_RSHIFT:
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
    case ui::VKEY_MENU:
    case ui::VKEY_LMENU:
    case ui::VKEY_RMENU:
    case ui::VKEY_LWIN:
    case ui::VKEY_RWIN:
    case ui::VKEY_ALTGR:
    case ui::VKEY_CAPITAL:
      return true;
    default:
      return false;
  }
}

}  // namespace

class UnboundedSurfaceWindowAura::DebugBorderDelegate
    : public ui::LayerDelegate {
 public:
  explicit DebugBorderDelegate(ui::Layer* layer) : layer_(layer) {}
  ~DebugBorderDelegate() override = default;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, layer_->size());
    recorder.canvas()->DrawSolidFocusRect(gfx::RectF(layer_->size()),
                                          SK_ColorRED, 3);
  }

  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

 private:
  raw_ptr<ui::Layer> layer_;
};

// static
std::unique_ptr<UnboundedSurfaceWindowAura> UnboundedSurfaceWindowAura::Create(
    RenderWidgetHostViewAura* parent_view,
    const gfx::Rect& bounds_in_screen,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view) {
  auto window = base::WrapUnique(
      new UnboundedSurfaceWindowAura(parent_view, std::move(subframe_view)));
  if (!window->InitWindow(bounds_in_screen)) {
    return nullptr;
  }
  return window;
}

UnboundedSurfaceWindowAura::UnboundedSurfaceWindowAura(
    RenderWidgetHostViewAura* parent_view,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view)
    : UnboundedSurfaceWindow(parent_view ? parent_view->GetWeakPtr() : nullptr,
                             std::move(subframe_view)) {
}

UnboundedSurfaceWindowAura::~UnboundedSurfaceWindowAura() {
  // `aura::Env` may already be gone during shutdown.
  if (aura::Env::HasInstance()) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  }
  StopObservingRootWindow();
  if (window_) {
    window_.reset();
  }
  if (frame_sink_id_.is_valid()) {
    if (parent_frame_sink_id_.is_valid()) {
      GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
          parent_frame_sink_id_, frame_sink_id_);
    }
    GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_, this, {});
  }

  // Explicitly destroy the debug border components in a safe order to prevent
  // dangling raw_ptrs in both directions (layer -> delegate and delegate ->
  // layer).
  if (debug_border_layer_) {
    debug_border_layer_->set_delegate(nullptr);
  }
  debug_border_delegate_.reset();
  debug_border_layer_.reset();
}

base::WeakPtr<UnboundedSurfaceWindow> UnboundedSurfaceWindowAura::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool UnboundedSurfaceWindowAura::IsValid() const {
  return window_ != nullptr;
}

gfx::NativeWindow UnboundedSurfaceWindowAura::GetNativeWindow() const {
  return window_.get();
}

viz::FrameSinkId UnboundedSurfaceWindowAura::GetFrameSinkId() const {
  return frame_sink_id_;
}

viz::LocalSurfaceId UnboundedSurfaceWindowAura::GetLocalSurfaceId() const {
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

gfx::Rect UnboundedSurfaceWindowAura::GetBounds() const {
  return window_ ? window_->GetBoundsInScreenWithoutTransform() : gfx::Rect();
}

void UnboundedSurfaceWindowAura::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::TimeDelta timeout,
    base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback) {
  if (!window_ || !window_->layer()) {
    std::move(callback).Run(content::CopyFromSurfaceResult());
    return;
  }
  auto request = std::make_unique<viz::CopyOutputRequest>(
      viz::CopyOutputRequest::ResultFormat::RGBA,
      viz::CopyOutputRequest::ResultDestination::kSystemMemory,
      base::BindOnce(
          [](base::OnceCallback<void(const content::CopyFromSurfaceResult&)>
                 callback,
             std::unique_ptr<viz::CopyOutputResult> result) {
            std::move(callback).Run(
                ToCopyFromSurfaceResult(result->ScopedAccessSkBitmap()
                                            .GetOutScopedBitmapAndMetadata()));
          },
          std::move(callback)));
  request->set_result_task_runner(
      base::SingleThreadTaskRunner::GetCurrentDefault());

  window_->layer()->RequestCopyOfOutput(std::move(request));
  if (window_->layer()->GetCompositor()) {
    window_->layer()->GetCompositor()->ScheduleFullRedraw();
  }
}

void UnboundedSurfaceWindowAura::EnsureSurfaceSynchronizedForWebTest() {
  if (window_ && window_->layer()) {
    window_->layer()->AsSurface()->SetShowSurface(
        viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
        window_->GetBoundsInScreen().size(),
        cc::DeadlinePolicy::UseInfiniteDeadline(),
        /*stretch_content_to_fill_bounds=*/false);
  }
}

gfx::Size UnboundedSurfaceWindowAura::GetMinimumSize() const {
  return gfx::Size();
}

std::optional<gfx::Size> UnboundedSurfaceWindowAura::GetMaximumSize() const {
  return std::nullopt;
}

gfx::NativeCursor UnboundedSurfaceWindowAura::GetCursor(
    const gfx::Point& point) {
  return gfx::NativeCursor();
}

int UnboundedSurfaceWindowAura::GetNonClientComponent(
    const gfx::Point& point) const {
  return HTCLIENT;
}

bool UnboundedSurfaceWindowAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return false;
}

bool UnboundedSurfaceWindowAura::CanFocus() {
  return false;
}

void UnboundedSurfaceWindowAura::OnWindowDestroying(aura::Window* window) {
  if (window == window_.get()) {
    // Relinquish ownership of window_ so that when UnboundedSurfaceWindowAura
    // is destructed, it does not attempt to double-free or re-entrantly destroy
    // the aura::Window.
    window_.release();
    NotifyWindowDestroyed();
    // The native window is gone, so `this` is useless: remove it from the
    // root view, so that it no longer reports an active surface.
    ScheduleDeferredDestroy();
  } else if (window == root_window_) {
    StopObservingRootWindow();
  }
}

void UnboundedSurfaceWindowAura::StopObservingRootWindow() {
  if (!root_window_) {
    return;
  }
  // The activation client may have been destroyed by the time we get here.
  if (wm::ActivationClient* activation_client =
          wm::GetActivationClient(root_window_)) {
    activation_client->RemoveObserver(this);
  }
  root_window_->RemoveObserver(this);
  root_window_ = nullptr;
}

bool UnboundedSurfaceWindowAura::IsInOwnerView(aura::Window* window) const {
  if (!window) {
    return false;
  }
  if (window_ && (window_->Contains(window) ||
                  wm::HasTransientAncestor(window, window_.get()))) {
    return true;
  }
  aura::Window* parent_native_view =
      parent_view_ ? parent_view_->GetNativeView() : nullptr;
  if (parent_native_view &&
      (parent_native_view->Contains(window) ||
       wm::HasTransientAncestor(window, parent_native_view))) {
    return true;
  }
  return false;
}

bool UnboundedSurfaceWindowAura::HasHitTestMask() const {
  return false;
}

bool UnboundedSurfaceWindowAura::InitWindow(const gfx::Rect& bounds_in_screen) {
  if (!parent_view_) {
    return false;
  }
  aura::Window* parent_native_view = parent_view_->GetNativeView();
  if (!parent_native_view) {
    return false;
  }

  aura::Window* root = parent_native_view->GetRootWindow();
  if (!root) {
    return false;
  }

  root_window_ = root;
  root_window_->AddObserver(this);
  // Dismiss when the window hosting the owner view is deactivated.
  if (wm::ActivationClient* activation_client =
          wm::GetActivationClient(root_window_)) {
    activation_client->AddObserver(this);
  }
  // Observe events for all root windows, not only `root_window_`, so that a
  // click or key press in another window (e.g. another browser window) also
  // dismisses the surface. This matches `MenuPreTargetHandlerAura`.
  aura::Env::GetInstance()->AddPreTargetHandler(
      this, ui::EventTarget::Priority::kSystem);

  frame_sink_id_ = content::AllocateFrameSinkId();

  window_ =
      std::make_unique<aura::Window>(this, aura::client::WINDOW_TYPE_MENU);
  window_->Init(ui::LayerType::LAYER_SURFACE);
  window_->SetTransparent(true);
  // TODO(crbug.com/508672616): Note that we may need to change this to a non-
  // transparent background later, if security issues arise. For example, this
  // allows content to put up a fully transparent (invisible) overlay over site
  // content and steal clicks/events.
  window_->layer()->AsSurface()->SetFallbackBackgroundColor(
      SkColors::kTransparent);
  window_->SetEmbedFrameSinkId(frame_sink_id_);

  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "UnboundedSurfaceWindow");

  if (parent_view_->host()) {
    parent_frame_sink_id_ = parent_view_->host()->GetFrameSinkId();
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(parent_frame_sink_id_,
                                                          frame_sink_id_);
  }

  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  if (transient_window_client && parent_native_view) {
    transient_window_client->AddTransientChild(parent_native_view,
                                               window_.get());
  }

  aura::client::ParentWindowWithContext(window_.get(), root, bounds_in_screen,
                                        display::kInvalidDisplayId);

  local_surface_id_allocator_.GenerateId();

  gfx::Rect relative_bounds =
      ConvertRectFromScreen(window_.get(), bounds_in_screen);
  window_->SetBounds(relative_bounds);
  // TODO(crbug.com/508672616): See the note above about transparent background.
  window_->layer()->AsSurface()->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnboundedWindowDebug)) {
    debug_border_layer_ = std::make_unique<ui::LayerTextured>();
    debug_border_layer_->SetName("UnboundedSurfaceWindow:DebugBorder");
    debug_border_delegate_ =
        std::make_unique<DebugBorderDelegate>(debug_border_layer_.get());
    debug_border_layer_->set_delegate(debug_border_delegate_.get());
    debug_border_layer_->SetBounds(gfx::Rect(window_->layer()->size()));
    debug_border_layer_->SetFillsBoundsOpaquely(false);
    debug_border_layer_->SetAcceptEvents(false);
    window_->layer()->Add(debug_border_layer_.get());
  }

  window_->Show();
  return true;
}

void UnboundedSurfaceWindowAura::SetBounds(const gfx::Rect& bounds_in_screen) {
  if (!window_) {
    return;
  }
  gfx::Rect relative_bounds =
      ConvertRectFromScreen(window_.get(), bounds_in_screen);
  window_->SetBounds(relative_bounds);
  local_surface_id_allocator_.GenerateId();
  window_->layer()->AsSurface()->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);
  if (debug_border_layer_) {
    debug_border_layer_->SetBounds(gfx::Rect(window_->layer()->size()));
  }
}

void UnboundedSurfaceWindowAura::TeardownAndDestroy() {
  if (parent_view_) {
    parent_view_->DestroyUnboundedSurface(GetWeakPtr());
  }
}

void UnboundedSurfaceWindowAura::GetCompositorFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) {
  GetHostFrameSinkManager()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink), std::move(client),
      /*render_input_router_config=*/nullptr);
}

void UnboundedSurfaceWindowAura::OnKeyEvent(ui::KeyEvent* event) {
  auto* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_ESCAPE) {
    // Only consume Escape in the window that hosts the surface, so that
    // Escape still reaches unrelated windows (e.g. another browser window).
    bool in_root_window = root_window_ && root_window_->Contains(target);
    Dismiss();
    if (in_root_window) {
      event->SetHandled();
    }
    return;
  }

  RenderWidgetHostViewBase* target_view = GetTargetView();
  if (window_ && target == window_.get() && target_view &&
      target_view->host()) {
    input::NativeWebKeyboardEvent web_event(*event);
    target_view->host()->ForwardKeyboardEvent(web_event);
    event->SetHandled();
    return;
  }

  // Dismiss on a key press aimed outside the owner view (e.g. a browser
  // shortcut while focus is in the omnibox or another tab), but let the event
  // continue to its target. Key presses inside the owner view are handled by
  // `RenderWidgetHostImpl`, which dismisses only for consumed shortcuts.
  if (event->type() == ui::EventType::kKeyPressed &&
      !IsModifierKey(event->key_code()) && !IsInOwnerView(target)) {
    Dismiss();
  }
}

void UnboundedSurfaceWindowAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed &&
      !IsInOwnerView(static_cast<aura::Window*>(event->target()))) {
    Dismiss();
    return;
  }

  if (window_ &&
      window_->Contains(static_cast<aura::Window*>(event->target()))) {
    if (event->type() == ui::EventType::kMousewheel) {
      blink::WebMouseWheelEvent web_event =
          ui::MakeWebMouseWheelEvent(*event->AsMouseWheelEvent());
      RouteMouseWheelEvent(web_event);
    } else {
      blink::WebMouseEvent web_event = ui::MakeWebMouseEvent(*event);
      RouteMouseEvent(web_event);
    }
    event->SetHandled();
  }
}

void UnboundedSurfaceWindowAura::OnScrollEvent(ui::ScrollEvent* event) {
  if (window_ &&
      window_->Contains(static_cast<aura::Window*>(event->target()))) {
    if (event->type() == ui::EventType::kScroll) {
      blink::WebMouseWheelEvent web_event = ui::MakeWebMouseWheelEvent(*event);
      RouteMouseWheelEvent(web_event);
      event->SetHandled();
    }
  }
}

void UnboundedSurfaceWindowAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchPressed) {
    auto* target = static_cast<aura::Window*>(event->target());
    if (!IsInOwnerView(target)) {
      Dismiss();
      return;
    }
    aura::Window* parent_native_view =
        parent_view_ ? parent_view_->GetNativeView() : nullptr;
    if (window_ && target == parent_native_view) {
      if (!window_->GetBoundsInRootWindow().Contains(event->root_location())) {
        Dismiss();
        return;
      }
    }
  }

  if (!window_ ||
      !window_->Contains(static_cast<aura::Window*>(event->target()))) {
    return;
  }

  RenderWidgetHostViewBase* target_view = GetTargetView();
  if (!target_view || !target_view->host()) {
    return;
  }

  if (!pointer_state_.OnTouch(*event)) {
    event->StopPropagation();
    return;
  }

  if (!target_view->GetRootView()) {
    return;
  }
  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, event->may_cause_scrolling(), event->hovering());
  pointer_state_.CleanupRemovedTouchPoints(*event);

  for (unsigned int i = 0; i < touch_event.touches_length; ++i) {
    blink::WebTouchPoint& touch_point = touch_event.touches[i];
    if (std::optional<gfx::PointF> local_point =
            TransformScreenPointToViewCoordSpace(
                target_view, touch_point.PositionInScreen())) {
      touch_point.SetPositionInWidget(local_point->x(), local_point->y());
    }
  }

  ui::LatencyInfo latency_info =
      event->latency() ? *event->latency() : ui::LatencyInfo();
  target_view->ProcessTouchEvent(touch_event, latency_info);
  // Disable synchronous handling to allow gesture generation after ACK.
  event->DisableSynchronousHandling();
}

void UnboundedSurfaceWindowAura::OnCancelMode(ui::CancelModeEvent* event) {
  Dismiss();
}

void UnboundedSurfaceWindowAura::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!window_) {
    return;
  }
  // Keep the surface open while activation stays within the window hosting
  // the owner view (e.g. the browser window), or moves into the surface.
  aura::Window* parent_native_view =
      parent_view_ ? parent_view_->GetNativeView() : nullptr;
  if (gained_active &&
      (IsInOwnerView(gained_active) ||
       (parent_native_view && gained_active->Contains(parent_native_view)))) {
    return;
  }
  Dismiss();
}

}  // namespace content

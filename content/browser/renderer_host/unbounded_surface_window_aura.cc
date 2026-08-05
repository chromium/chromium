// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window_aura.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect_conversions.h"

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
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    const gfx::Rect& bounds_in_screen,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view) {
  auto window = base::WrapUnique(new UnboundedSurfaceWindowAura(
      parent_view, std::move(host), std::move(client),
      std::move(subframe_view)));
  if (!window->InitWindow(bounds_in_screen)) {
    return nullptr;
  }
  return window;
}

UnboundedSurfaceWindowAura::UnboundedSurfaceWindowAura(
    RenderWidgetHostViewAura* parent_view,
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view)
    : parent_view_(parent_view), subframe_view_(std::move(subframe_view)) {
  if (host.is_valid()) {
    receiver_.Bind(std::move(host));
    receiver_.set_disconnect_handler(
        base::BindOnce(&UnboundedSurfaceWindowAura::OnConnectionError,
                       base::Unretained(this)));
  }
  if (client.is_valid()) {
    client_remote_.Bind(std::move(client));
  }
}

UnboundedSurfaceWindowAura::~UnboundedSurfaceWindowAura() {
  if (root_window_) {
    root_window_->RemovePreTargetHandler(this);
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }
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

bool UnboundedSurfaceWindowAura::is_valid() const {
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
    window_->layer()->SetShowSurface(
        viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
        window_->GetBoundsInScreen().size(), SkColors::kTransparent,
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
  } else if (window == root_window_) {
    root_window_->RemovePreTargetHandler(this);
    root_window_->RemoveObserver(this);
    root_window_ = nullptr;
  }
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
  root_window_->AddPreTargetHandler(this);

  frame_sink_id_ = content::AllocateFrameSinkId();

  window_ =
      std::make_unique<aura::Window>(this, aura::client::WINDOW_TYPE_MENU);
  window_->Init(ui::LayerType::LAYER_SOLID_COLOR);
  window_->SetTransparent(true);
  // TODO(crbug.com/508672616): Note that we may need to change this to a non-
  // transparent background later, if security issues arise. For example, this
  // allows content to put up a fully transparent (invisible) overlay over site
  // content and steal clicks/events.
  window_->layer()->AsSolidColor()->SetColor(SkColors::kTransparent);
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
  window_->layer()->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), SkColors::kTransparent,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnboundedWindowDebug)) {
    debug_border_layer_ = std::make_unique<ui::LayerTextured>();
    debug_border_delegate_ =
        std::make_unique<DebugBorderDelegate>(debug_border_layer_.get());
    debug_border_layer_->set_delegate(debug_border_delegate_.get());
    debug_border_layer_->SetBounds(gfx::Rect(window_->layer()->size()));
    debug_border_layer_->SetFillsBoundsOpaquely(false);
    debug_border_layer_->SetAcceptEvents(false);
    window_->layer()->Add(debug_border_layer_.get());
  }

  window_->Show();

  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }
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
  window_->layer()->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), SkColors::kTransparent,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);
  if (debug_border_layer_) {
    debug_border_layer_->SetBounds(gfx::Rect(window_->layer()->size()));
  }
}

void UnboundedSurfaceWindowAura::UpdateBounds(const gfx::Rect& bounds) {
  if (!parent_view_) {
    return;
  }
  parent_view_->UpdateUnboundedSurfaceBoundsInSubframeContext(
      bounds, subframe_view_.get());
  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }
}

void UnboundedSurfaceWindowAura::Dismiss() {
  if (client_remote_.is_bound()) {
    client_remote_->OnDismissed();
    client_remote_.reset();
  }
  if (parent_view_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RenderWidgetHostViewBase::DestroyUnboundedSurface,
                       parent_view_->GetWeakPtr(), GetWeakPtr()));
  }
}

void UnboundedSurfaceWindowAura::OnConnectionError() {
  Dismiss();
}

void UnboundedSurfaceWindowAura::GetCompositorFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) {
  GetHostFrameSinkManager()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink), std::move(client),
      /*render_input_router_config=*/nullptr);
}

void UnboundedSurfaceWindowAura::RouteMouseEvent(
    const blink::WebMouseEvent& event) {
  if (!parent_view_ || !parent_view_->host() ||
      !parent_view_->host()->delegate() ||
      !parent_view_->host()->delegate()->GetInputEventRouter()) {
    return;
  }
  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(parent_view_->GetRootView());
  if (!root_view) {
    return;
  }

  blink::WebMouseEvent web_event = event;
  gfx::PointF screen_point(web_event.PositionInScreen());
  gfx::Point root_origin = root_view->GetViewBounds().origin();
  gfx::PointF root_point =
      screen_point - gfx::Vector2dF(root_origin.x(), root_origin.y());
  gfx::PointF parent_local_point =
      parent_view_->TransformRootPointToViewCoordSpace(root_point);
  web_event.SetPositionInWidget(parent_local_point.x(), parent_local_point.y());

  parent_view_->host()->delegate()->GetInputEventRouter()->RouteMouseEvent(
      parent_view_, &web_event, ui::LatencyInfo());
}

void UnboundedSurfaceWindowAura::OnKeyEvent(ui::KeyEvent* event) {
  if (event->type() == ui::EventType::kKeyPressed &&
      event->key_code() == ui::VKEY_ESCAPE) {
    Dismiss();
    event->SetHandled();
    return;
  }

  if (window_ && event->target() == window_.get() && parent_view_ &&
      parent_view_->host()) {
    input::NativeWebKeyboardEvent web_event(*event);
    parent_view_->host()->ForwardKeyboardEvent(web_event);
    event->SetHandled();
  }
}

void UnboundedSurfaceWindowAura::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed &&
      (!window_ ||
       !window_->Contains(static_cast<aura::Window*>(event->target())))) {
    Dismiss();
    return;
  }

  if (window_ &&
      window_->Contains(static_cast<aura::Window*>(event->target()))) {
    blink::WebMouseEvent web_event = ui::MakeWebMouseEvent(*event);
    RouteMouseEvent(web_event);
    event->SetHandled();
  }
}

void UnboundedSurfaceWindowAura::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::EventType::kTouchPressed &&
      (!window_ ||
       !window_->Contains(static_cast<aura::Window*>(event->target())))) {
    Dismiss();
    return;
  }

  if (!window_ ||
      !window_->Contains(static_cast<aura::Window*>(event->target()))) {
    return;
  }

  if (!parent_view_ || !parent_view_->host() ||
      !parent_view_->host()->delegate() ||
      !parent_view_->host()->delegate()->GetInputEventRouter()) {
    return;
  }
  input::RenderWidgetHostInputEventRouter* router =
      parent_view_->host()->delegate()->GetInputEventRouter();

  if (!pointer_state_.OnTouch(*event)) {
    event->StopPropagation();
    return;
  }

  aura::Window* parent_window = parent_view_->GetNativeView();
  if (!parent_window || !parent_window->GetRootWindow()) {
    return;
  }
  blink::WebTouchEvent touch_event = ui::CreateWebTouchEventFromMotionEvent(
      pointer_state_, event->may_cause_scrolling(), event->hovering());
  pointer_state_.CleanupRemovedTouchPoints(*event);

  for (unsigned int i = 0; i < touch_event.touches_length; ++i) {
    blink::WebTouchPoint& touch_point = touch_event.touches[i];
    gfx::PointF parent_local_point = touch_point.PositionInScreen();
    // Since the touch event's PositionInScreen is populated from the
    // MotionEvent's raw coordinates which are in root window space, not screen
    // space, we must convert from root window coordinates to parent local
    // coordinates.
    aura::Window::ConvertPointToTarget(parent_window->GetRootWindow(),
                                       parent_window, &parent_local_point);
    touch_point.SetPositionInWidget(parent_local_point.x(),
                                    parent_local_point.y());
  }

  ui::LatencyInfo latency_info =
      event->latency() ? *event->latency() : ui::LatencyInfo();
  router->RouteTouchEvent(parent_view_, &touch_event, latency_info);
  // Disable synchronous handling to allow gesture generation after ACK.
  event->DisableSynchronousHandling();
}

}  // namespace content

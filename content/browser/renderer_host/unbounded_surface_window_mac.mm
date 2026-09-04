// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/apple/owned_objc.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/web_input_event_builders_mac.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/host/host_frame_sink_manager.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#import "ui/base/cocoa/base_view.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/latency/latency_info.h"

// UnboundedNSView inherits from BaseView, which installs a CrTrackingArea in
// its constructor. On macOS, floating and non-key windows do not receive mouse
// moved events unless an active tracking area is installed on a view within the
// window. BaseView funnels mouse and key events to -mouseEvent: and -keyEvent:,
// which route events to the owner.
@interface UnboundedNSView : BaseView {
  raw_ptr<content::UnboundedSurfaceWindowMac> _owner;
}
- (instancetype)initWithFrame:(NSRect)frame
                        owner:(content::UnboundedSurfaceWindowMac*)owner;
- (void)clearOwner;
@end

@implementation UnboundedNSView
- (instancetype)initWithFrame:(NSRect)frame
                        owner:(content::UnboundedSurfaceWindowMac*)owner {
  if (self = [super initWithFrame:frame tracking:YES]) {
    _owner = owner;
  }
  return self;
}

- (void)clearOwner {
  _owner = nullptr;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  if (_owner) {
    _owner->RouteMouseEvent(theEvent);
  }
}

- (void)scrollWheel:(NSEvent*)theEvent {
  if (_owner) {
    _owner->RouteWheelEvent(theEvent);
  }
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  if (_owner) {
    _owner->RouteKeyboardEvent(theEvent);
    return kEventHandled;
  }
  return kEventNotHandled;
}
@end

@interface UnboundedNSWindow : NSWindow
@end

@implementation UnboundedNSWindow
- (BOOL)canBecomeKeyWindow {
  return NO;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}
@end

namespace content {

namespace {

template <typename T>
RenderWidgetHostViewMac* TranslateCoordinatesToRoot(
    RenderWidgetHostViewMac* parent_view,
    T& web_event) {
  if (!parent_view) {
    return nullptr;
  }
  auto* root_view =
      static_cast<RenderWidgetHostViewMac*>(parent_view->GetRootView());
  if (!root_view) {
    return nullptr;
  }

  gfx::Point root_origin = root_view->GetViewBounds().origin();
  gfx::PointF root_point = web_event.PositionInScreen() -
                           gfx::Vector2dF(root_origin.x(), root_origin.y());
  web_event.SetPositionInWidget(root_point.x(), root_point.y());

  return root_view;
}

}  // namespace

UnboundedSurfaceWindowMac::UnboundedSurfaceWindowMac(
    RenderWidgetHostViewMac* parent_view,
    mojo::PendingAssociatedReceiver<blink::mojom::UnboundedSurfaceHost> host,
    mojo::PendingAssociatedRemote<blink::mojom::UnboundedSurfaceClient> client,
    const gfx::Rect& bounds_in_screen,
    base::WeakPtr<RenderWidgetHostViewBase> subframe_view)
    : parent_view_(parent_view),
      subframe_view_(std::move(subframe_view)),
      frame_sink_id_(content::AllocateFrameSinkId()) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kUnboundedElement),
        base::NotFatalUntil::M152);
  if (host.is_valid()) {
    receiver_.Bind(std::move(host));
    receiver_.set_disconnect_handler(base::BindOnce(
        &UnboundedSurfaceWindowMac::OnConnectionError, base::Unretained(this)));
  }
  if (client.is_valid()) {
    client_remote_.Bind(std::move(client));
  }
  InitWindow(bounds_in_screen);
}

bool UnboundedSurfaceWindowMac::IsValid() const {
  return window_ != nil;
}

gfx::NativeWindow UnboundedSurfaceWindowMac::GetNativeWindow() const {
  return gfx::NativeWindow(window_);
}

UnboundedSurfaceWindowMac::~UnboundedSurfaceWindowMac() {
  if (parent_view_ && parent_view_->host()) {
    GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
        parent_view_->host()->GetFrameSinkId(), frame_sink_id_);
  }
  if (recyclable_compositor_) {
    GetHostFrameSinkManager()->UnregisterFrameSinkHierarchy(
        recyclable_compositor_->compositor()->frame_sink_id(), frame_sink_id_);
    recyclable_compositor_->widget()->ResetNSView();
    recyclable_compositor_->compositor()->SetRootLayer(nullptr);
    recyclable_compositor_.reset();
  }
  display_ca_layer_tree_.reset();
  root_layer_.reset();

  if (window_) {
    UnboundedNSView* view =
        base::apple::ObjCCast<UnboundedNSView>([window_ contentView]);
    [view clearOwner];
    if (NSWindow* parent = [window_ parentWindow]) {
      [parent removeChildWindow:window_];
    }
    [window_ orderOut:nil];
    [window_ close];
    window_ = nil;
  }
  GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_, this, {});
}

base::WeakPtr<UnboundedSurfaceWindow> UnboundedSurfaceWindowMac::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

UnboundedSurfaceWindowMac::DisplayInfo
UnboundedSurfaceWindowMac::GetDisplayInfo() const {
  DisplayInfo info;
  if (parent_view_) {
    display::ScreenInfo screen_info = parent_view_->GetScreenInfo();
    info.scale_factor = screen_info.device_scale_factor;
    info.display_color_spaces = screen_info.display_color_spaces;
    info.display_id = screen_info.display_id;
    info.display_frequency = screen_info.display_frequency;
  } else {
    display::Display display = display::Screen::Get()->GetPrimaryDisplay();
    info.scale_factor = display.device_scale_factor();
    info.display_color_spaces = display.GetColorSpaces();
    info.display_id = display.id();
    info.display_frequency = display.display_frequency();
  }
  return info;
}

void UnboundedSurfaceWindowMac::InitWindow(const gfx::Rect& bounds_in_screen) {
  NSRect ns_rect = gfx::ScreenRectToNSRect(bounds_in_screen);

  window_ =
      [[UnboundedNSWindow alloc] initWithContentRect:ns_rect
                                           styleMask:NSWindowStyleMaskBorderless
                                             backing:NSBackingStoreBuffered
                                               defer:NO];
  [window_ setReleasedWhenClosed:NO];
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setOpaque:NO];
  [window_ setLevel:NSFloatingWindowLevel];
  [window_ setAcceptsMouseMovedEvents:YES];

  NSRect client_rect =
      NSMakeRect(0, 0, ns_rect.size.width, ns_rect.size.height);
  UnboundedNSView* content_view =
      [[UnboundedNSView alloc] initWithFrame:client_rect owner:this];
  [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  CALayer* background_layer = [CALayer layer];
  background_layer.backgroundColor = [[NSColor clearColor] CGColor];
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUnboundedWindowDebug)) {
    background_layer.borderColor = [[NSColor redColor] CGColor];
    background_layer.borderWidth = 3.0;
  }

  display_ca_layer_tree_ =
      std::make_unique<ui::DisplayCALayerTree>(background_layer);

  [content_view setLayer:background_layer];
  [content_view setWantsLayer:YES];
  [window_ setContentView:content_view];

  GetHostFrameSinkManager()->RegisterFrameSinkId(
      frame_sink_id_, this, viz::ReportFirstSurfaceActivation::kNo);
  GetHostFrameSinkManager()->SetFrameSinkDebugLabel(frame_sink_id_,
                                                    "UnboundedSurfaceWindow");

  local_surface_id_allocator_.GenerateId();

  // Set up the recyclable compositor
  recyclable_compositor_ = std::make_unique<ui::RecyclableCompositorMac>(
      content::GetContextFactory());

  root_layer_ = std::make_unique<ui::LayerSurface>();
  root_layer_->SetFallbackBackgroundColor(SkColors::kTransparent);
  root_layer_->SetFillsBoundsOpaquely(false);
  root_layer_->SetBounds(gfx::Rect(bounds_in_screen.size()));

  DisplayInfo display_info = GetDisplayInfo();

  gfx::Size size_pixels = gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
      bounds_in_screen.size(), display_info.scale_factor));

  recyclable_compositor_->UpdateSurface(
      size_pixels, display_info.scale_factor, display_info.display_color_spaces,
      display_info.display_id, display_info.display_frequency);

  recyclable_compositor_->compositor()->SetRootLayer(root_layer_.get());
  recyclable_compositor_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);
  recyclable_compositor_->widget()->SetNSView(this);
  recyclable_compositor_->Unsuspend();

  GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
      recyclable_compositor_->compositor()->frame_sink_id(), frame_sink_id_);
  if (parent_view_ && parent_view_->host()) {
    GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
        parent_view_->host()->GetFrameSinkId(), frame_sink_id_);
  }

  root_layer_->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);

  if (parent_view_ && parent_view_->GetInProcessNSView()) {
    if (NSWindow* parent_window = [parent_view_->GetInProcessNSView() window]) {
      [parent_window addChildWindow:window_ ordered:NSWindowAbove];
      [window_ setLevel:[parent_window level] + 1];
    }
  }
  [window_ orderFront:nil];

  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }
}

void UnboundedSurfaceWindowMac::SetBounds(const gfx::Rect& bounds_in_screen) {
  if (window_) {
    NSRect ns_rect = gfx::ScreenRectToNSRect(bounds_in_screen);
    [window_ setFrame:ns_rect display:YES];
    [window_.contentView
        setFrame:NSMakeRect(0, 0, ns_rect.size.width, ns_rect.size.height)];
    local_surface_id_allocator_.GenerateId();

    if (root_layer_) {
      root_layer_->SetBounds(gfx::Rect(bounds_in_screen.size()));
    }

    if (recyclable_compositor_) {
      DisplayInfo display_info = GetDisplayInfo();

      gfx::Size size_pixels = gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
          bounds_in_screen.size(), display_info.scale_factor));

      recyclable_compositor_->UpdateSurface(
          size_pixels, display_info.scale_factor,
          display_info.display_color_spaces, display_info.display_id,
          display_info.display_frequency);
    }

    if (root_layer_) {
      root_layer_->SetShowSurface(
          viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
          bounds_in_screen.size(), cc::DeadlinePolicy::UseDefaultDeadline(),
          /*stretch_content_to_fill_bounds=*/false);
    }
  }
}

viz::FrameSinkId UnboundedSurfaceWindowMac::GetFrameSinkId() const {
  return frame_sink_id_;
}

viz::LocalSurfaceId UnboundedSurfaceWindowMac::GetLocalSurfaceId() const {
  return local_surface_id_allocator_.GetCurrentLocalSurfaceId();
}

void UnboundedSurfaceWindowMac::GetCompositorFrameSink(
    mojo::PendingReceiver<viz::mojom::CompositorFrameSink> sink,
    mojo::PendingRemote<viz::mojom::CompositorFrameSinkClient> client) {
  GetHostFrameSinkManager()->CreateCompositorFrameSink(
      frame_sink_id_, std::move(sink), std::move(client),
      /*render_input_router_config=*/nullptr);
}

gfx::Rect UnboundedSurfaceWindowMac::GetBounds() const {
  if (!window_) {
    return gfx::Rect();
  }
  return gfx::ScreenRectFromNSRect([window_ frame]);
}

void UnboundedSurfaceWindowMac::CopyFromSurface(
    const gfx::Rect& src_subrect,
    const gfx::Size& dst_size,
    base::TimeDelta timeout,
    base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback) {
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

  if (recyclable_compositor_ && recyclable_compositor_->compositor() &&
      recyclable_compositor_->compositor()->root_layer()) {
    recyclable_compositor_->compositor()->root_layer()->RequestCopyOfOutput(
        std::move(request));
    recyclable_compositor_->compositor()->ScheduleFullRedraw();
  } else if (root_layer_) {
    root_layer_->RequestCopyOfOutput(std::move(request));
  }
}

void UnboundedSurfaceWindowMac::EnsureSurfaceSynchronizedForWebTest() {
  if (root_layer_) {
    root_layer_->SetShowSurface(
        viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
        root_layer_->bounds().size(), cc::DeadlinePolicy::UseDefaultDeadline(),
        /*stretch_content_to_fill_bounds=*/false);
  }
  if (recyclable_compositor_ && recyclable_compositor_->compositor()) {
    DisplayInfo display_info = GetDisplayInfo();
    gfx::Size size_pixels = gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
        root_layer_->bounds().size(), display_info.scale_factor));
    recyclable_compositor_->UpdateSurface(
        size_pixels, display_info.scale_factor,
        display_info.display_color_spaces, display_info.display_id,
        display_info.display_frequency);
  }
}

RenderWidgetHostViewBase* UnboundedSurfaceWindowMac::GetParentView() const {
  return parent_view_;
}

void UnboundedSurfaceWindowMac::RouteMouseEvent(NSEvent* ns_event) {
  blink::WebMouseEvent web_event =
      input::WebMouseEventBuilder::Build(ns_event, window_.contentView);
  RenderWidgetHostViewMac* root_view =
      TranslateCoordinatesToRoot(parent_view_, web_event);
  if (!root_view) {
    return;
  }
  root_view->RouteOrProcessMouseEvent(web_event);
}

void UnboundedSurfaceWindowMac::RouteWheelEvent(NSEvent* ns_event) {
  blink::WebMouseWheelEvent web_event =
      input::WebMouseWheelEventBuilder::Build(ns_event, window_.contentView);
  RenderWidgetHostViewMac* root_view =
      TranslateCoordinatesToRoot(parent_view_, web_event);
  if (!root_view) {
    return;
  }
  root_view->RouteOrProcessWheelEvent(web_event);
}

void UnboundedSurfaceWindowMac::RouteKeyboardEvent(NSEvent* ns_event) {
  base::apple::OwnedNSEvent owned_event(ns_event);
  input::NativeWebKeyboardEvent web_event(owned_event);

  if (web_event.GetType() == blink::WebInputEvent::Type::kKeyDown &&
      web_event.windows_key_code == ui::VKEY_ESCAPE) {
    Dismiss();
    return;
  }

  if (parent_view_ && parent_view_->host()) {
    parent_view_->host()->ForwardKeyboardEvent(web_event);
  }
}

void UnboundedSurfaceWindowMac::UpdateBounds(const gfx::Rect& bounds) {
  if (!parent_view_) {
    return;
  }
  parent_view_->UpdateUnboundedSurfaceBoundsInSubframeContext(
      bounds, subframe_view_.get());
  if (client_remote_.is_bound()) {
    client_remote_->OnSurfaceAllocated(GetFrameSinkId(), GetLocalSurfaceId());
  }
}

void UnboundedSurfaceWindowMac::TeardownAndDestroy() {
  if (parent_view_) {
    parent_view_->DestroyUnboundedSurface(GetWeakPtr());
  }
}

void UnboundedSurfaceWindowMac::OnConnectionError() {
  dismiss_pending_ = true;
  ScheduleDeferredDestroy();
}

void UnboundedSurfaceWindowMac::AcceleratedWidgetCALayerParamsUpdated(
    gfx::CALayerParams ca_layer_params) {
  if (display_ca_layer_tree_) {
    display_ca_layer_tree_->UpdateCALayerTree(std::move(ca_layer_params));
  }
}

}  // namespace content

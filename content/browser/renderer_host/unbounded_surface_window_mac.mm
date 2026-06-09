// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/unbounded_surface_window_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/owned_objc.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/input/web_input_event_builders_mac.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/public/browser/context_factory.h"
#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/compositor/recyclable_compositor_mac.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/gfx/mac/coordinate_conversion.h"
#include "ui/latency/latency_info.h"

@interface UnboundedNSWindow : NSWindow {
  raw_ptr<content::UnboundedSurfaceWindowMac> _owner;
}
- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag
                              owner:(content::UnboundedSurfaceWindowMac*)owner;
- (void)clearOwner;
@end

@implementation UnboundedNSWindow
- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag
                              owner:(content::UnboundedSurfaceWindowMac*)owner {
  if (self = [super initWithContentRect:contentRect
                              styleMask:style
                                backing:backingStoreType
                                  defer:flag]) {
    _owner = owner;
  }
  return self;
}

- (void)clearOwner {
  _owner = nullptr;
}

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (void)resignKeyWindow {
  [super resignKeyWindow];
  if (_owner) {
    _owner->OnWindowResignedKey();
  }
}

- (void)sendEvent:(NSEvent*)event {
  if (_owner) {
    NSEventMask eventMask = NSEventMaskFromType(event.type);
    if (eventMask & (NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp |
                     NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
                     NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged |
                     NSEventMaskRightMouseDragged)) {
      _owner->RouteMouseEvent(event);
    } else if (eventMask & (NSEventMaskKeyDown | NSEventMaskKeyUp |
                            NSEventMaskFlagsChanged)) {
      _owner->RouteKeyboardEvent(event);
    }
  }
  [super sendEvent:event];
}
@end

namespace content {

UnboundedSurfaceWindowMac::UnboundedSurfaceWindowMac(
    RenderFrameHostImpl* parent_rfh,
    const gfx::Rect& bounds_in_screen)
    : parent_rfh_(parent_rfh), frame_sink_id_(content::AllocateFrameSinkId()) {
  InitWindow(bounds_in_screen);
}

bool UnboundedSurfaceWindowMac::is_valid() const {
  return window_ != nil;
}

UnboundedSurfaceWindowMac::~UnboundedSurfaceWindowMac() {
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
    [(UnboundedNSWindow*)window_ clearOwner];
    [window_ orderOut:nil];
    [window_ close];
    window_ = nil;
  }
  GetHostFrameSinkManager()->InvalidateFrameSinkId(frame_sink_id_, this, {});
}

UnboundedSurfaceWindowMac::DisplayInfo
UnboundedSurfaceWindowMac::GetDisplayInfo() const {
  DisplayInfo info;
  if (parent_rfh_ && parent_rfh_->GetRenderWidgetHost() &&
      parent_rfh_->GetRenderWidgetHost()->GetView()) {
    display::ScreenInfo screen_info =
        parent_rfh_->GetRenderWidgetHost()->GetView()->GetScreenInfo();
    info.scale_factor = screen_info.device_scale_factor;
    info.display_color_spaces = screen_info.display_color_spaces;
    info.display_id = screen_info.display_id;
  } else {
    display::Display display = display::Screen::Get()->GetPrimaryDisplay();
    info.scale_factor = display.device_scale_factor();
    info.display_color_spaces = display.GetColorSpaces();
    info.display_id = display.id();
  }
  return info;
}

void UnboundedSurfaceWindowMac::InitWindow(const gfx::Rect& bounds_in_screen) {
  NSRect ns_rect = gfx::ScreenRectToNSRect(bounds_in_screen);

  window_ =
      [[UnboundedNSWindow alloc] initWithContentRect:ns_rect
                                           styleMask:NSWindowStyleMaskBorderless
                                             backing:NSBackingStoreBuffered
                                               defer:NO
                                               owner:this];
  [window_ setReleasedWhenClosed:NO];
  [window_ setBackgroundColor:[NSColor clearColor]];
  [window_ setOpaque:NO];

  NSRect client_rect =
      NSMakeRect(0, 0, ns_rect.size.width, ns_rect.size.height);
  NSView* content_view = [[NSView alloc] initWithFrame:client_rect];
  [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

  CALayer* background_layer = [CALayer layer];
  background_layer.backgroundColor = [[NSColor clearColor] CGColor];

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

  root_layer_ = std::make_unique<ui::Layer>(ui::LayerType::LAYER_SOLID_COLOR);
  root_layer_->SetColor(SK_ColorTRANSPARENT);
  root_layer_->SetBounds(gfx::Rect(bounds_in_screen.size()));

  DisplayInfo display_info = GetDisplayInfo();

  gfx::Size size_pixels = gfx::ToRoundedSize(gfx::ConvertSizeToPixels(
      bounds_in_screen.size(), display_info.scale_factor));

  recyclable_compositor_->UpdateSurface(
      size_pixels, display_info.scale_factor, display_info.display_color_spaces,
      display_info.display_id,
      /*refresh_rate_changed_on_same_display=*/false);

  recyclable_compositor_->compositor()->SetRootLayer(root_layer_.get());
  recyclable_compositor_->compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);
  recyclable_compositor_->widget()->SetNSView(this);
  recyclable_compositor_->Unsuspend();

  GetHostFrameSinkManager()->RegisterFrameSinkHierarchy(
      recyclable_compositor_->compositor()->frame_sink_id(), frame_sink_id_);

  root_layer_->SetShowSurface(
      viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
      bounds_in_screen.size(), SK_ColorTRANSPARENT,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      /*stretch_content_to_fill_bounds=*/false);

  [window_ orderFront:nil];
  [window_ makeKeyAndOrderFront:nil];
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
          /*refresh_rate_changed_on_same_display=*/false);
    }

    if (root_layer_) {
      root_layer_->SetShowSurface(
          viz::SurfaceId(frame_sink_id_, GetLocalSurfaceId()),
          bounds_in_screen.size(), SK_ColorTRANSPARENT,
          cc::DeadlinePolicy::UseDefaultDeadline(),
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

gfx::Rect UnboundedSurfaceWindowMac::GetBoundsForTesting() const {
  if (!window_) {
    return gfx::Rect();
  }
  return gfx::ScreenRectFromNSRect([window_ frame]);
}

void UnboundedSurfaceWindowMac::RouteMouseEvent(NSEvent* ns_event) {
  RouteMouseEvent(
      input::WebMouseEventBuilder::Build(ns_event, window_.contentView));
}

void UnboundedSurfaceWindowMac::RouteMouseEvent(
    const blink::WebMouseEvent& event) {
  if (!parent_rfh_ || !parent_rfh_->GetRenderWidgetHost() ||
      !parent_rfh_->GetRenderWidgetHost()->GetView() ||
      !parent_rfh_->GetRenderWidgetHost()->delegate() ||
      !parent_rfh_->GetRenderWidgetHost()->delegate()->GetInputEventRouter()) {
    return;
  }
  RenderWidgetHostViewBase* parent_view =
      static_cast<RenderWidgetHostViewBase*>(
          parent_rfh_->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* root_view =
      static_cast<RenderWidgetHostViewBase*>(parent_view->GetRootView());
  if (!root_view) {
    return;
  }

  blink::WebMouseEvent web_event = event;
  gfx::PointF screen_point(web_event.PositionInScreen());
  gfx::Point root_origin = root_view->GetViewBounds().origin();
  gfx::PointF root_point =
      screen_point - gfx::Vector2dF(root_origin.x(), root_origin.y());
  gfx::PointF parent_local_point =
      parent_view->TransformRootPointToViewCoordSpace(root_point);
  web_event.SetPositionInWidget(parent_local_point.x(), parent_local_point.y());

  parent_rfh_->GetRenderWidgetHost()
      ->delegate()
      ->GetInputEventRouter()
      ->RouteMouseEvent(parent_view, &web_event, ui::LatencyInfo());
}

void UnboundedSurfaceWindowMac::RouteKeyboardEvent(NSEvent* ns_event) {
  base::apple::OwnedNSEvent owned_event(ns_event);
  input::NativeWebKeyboardEvent web_event(owned_event);

  if (web_event.GetType() == blink::WebInputEvent::Type::kKeyDown &&
      web_event.windows_key_code == ui::VKEY_ESCAPE) {
    if (parent_rfh_) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&RenderFrameHostImpl::DismissUnboundedSurface,
                         parent_rfh_->GetWeakPtr()));
    }
    return;
  }

  if (parent_rfh_ && parent_rfh_->GetRenderWidgetHost()) {
    parent_rfh_->GetRenderWidgetHost()->ForwardKeyboardEvent(web_event);
  }
}

void UnboundedSurfaceWindowMac::OnWindowResignedKey() {
  if (parent_rfh_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&RenderFrameHostImpl::DismissUnboundedSurface,
                                  parent_rfh_->GetWeakPtr()));
  }
}

void UnboundedSurfaceWindowMac::AcceleratedWidgetCALayerParamsUpdated(
    gfx::CALayerParams ca_layer_params) {
  if (display_ca_layer_tree_) {
    display_ca_layer_tree_->UpdateCALayerTree(std::move(ca_layer_params));
  }
}

}  // namespace content

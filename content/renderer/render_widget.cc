// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/ukm_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_page_popup.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace content {

namespace {

RenderWidget::CreateRenderWidgetFunction g_create_render_widget_for_frame =
    nullptr;

}  // namespace

// RenderWidget ---------------------------------------------------------------

// static
void RenderWidget::InstallCreateForFrameHook(
    CreateRenderWidgetFunction create_widget) {
  g_create_render_widget_for_frame = create_widget;
}

std::unique_ptr<RenderWidget> RenderWidget::CreateForFrame(
    CompositorDependencies* compositor_deps) {
  if (g_create_render_widget_for_frame) {
    return g_create_render_widget_for_frame(compositor_deps);
  }

  return std::make_unique<RenderWidget>(compositor_deps);
}

RenderWidget* RenderWidget::CreateForPopup(
    CompositorDependencies* compositor_deps) {
  return new RenderWidget(compositor_deps);
}

RenderWidget::RenderWidget(CompositorDependencies* compositor_deps)
    : compositor_deps_(compositor_deps) {
  DCHECK(RenderThread::IsMainThread());
  DCHECK(compositor_deps_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  DCHECK(closing_)
      << " RenderWidget must be destroyed via RenderWidget::Close()";
}

void RenderWidget::InitForPopup(RenderWidget* opener_widget,
                                blink::WebPagePopup* web_page_popup,
                                const blink::ScreenInfo& screen_info) {
  for_popup_ = true;
  Initialize(web_page_popup, screen_info);
}

void RenderWidget::InitForMainFrame(blink::WebFrameWidget* web_frame_widget,
                                    const blink::ScreenInfo& screen_info,
                                    RenderWidgetDelegate& delegate) {
  delegate_ = &delegate;
  Initialize(web_frame_widget, screen_info);
}

void RenderWidget::InitForChildLocalRoot(
    blink::WebFrameWidget* web_frame_widget,
    const blink::ScreenInfo& screen_info) {
  for_child_local_root_frame_ = true;
  Initialize(web_frame_widget, screen_info);
}

void RenderWidget::CloseForFrame(std::unique_ptr<RenderWidget> widget) {
  DCHECK(for_frame());
  DCHECK_EQ(widget.get(), this);  // This method takes ownership of |this|.

  Close(std::move(widget));
}

void RenderWidget::Initialize(blink::WebWidget* web_widget,
                              const blink::ScreenInfo& screen_info) {
  DCHECK(web_widget);

  webwidget_ = web_widget;
  if (auto* scheduler_state = GetWebWidget()->RendererWidgetSchedulingState())
    scheduler_state->SetHidden(web_widget->IsHidden());

  InitCompositing(screen_info);

  // If the widget is hidden, delay starting the compositor until the user
  // shows it. Otherwise start the compositor immediately. If the widget is
  // for a provisional frame, this importantly starts the compositor before
  // the frame is inserted into the frame tree, which impacts first paint
  // metrics.
  if (!web_widget->IsHidden())
    web_widget->SetCompositorVisible(true);
}

void RenderWidget::BrowserClosedIpcChannelForPopupWidget() {
  DCHECK(for_popup_);

  Close(base::WrapUnique(this));
}

void RenderWidget::ScheduleAnimation() {
  // This call is not needed in single thread mode for tests without a
  // scheduler, but they override this method in order to schedule a synchronous
  // composite task themselves.
  // TODO(dtapuska): ScheduleAnimation might get called before layer_tree_host_
  // is assigned, inside the InitializeCompositing call. This should eventually
  // go away when this is moved inside blink. https://crbug.com/1097816
  if (layer_tree_host_)
    layer_tree_host_->SetNeedsAnimate();
}

void RenderWidget::UpdateTextInputState() {
  GetWebWidget()->UpdateTextInputState();
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::SetHandlingInputEvent(bool handling_input_event) {
  GetWebWidget()->SetHandlingInputEvent(handling_input_event);
}

void RenderWidget::InitCompositing(const blink::ScreenInfo& screen_info) {
  TRACE_EVENT0("blink", "RenderWidget::InitializeLayerTreeView");

  layer_tree_host_ = webwidget_->InitializeCompositing(
      compositor_deps_->GetWebMainThreadScheduler(),
      compositor_deps_->GetTaskGraphRunner(), for_child_local_root_frame_,
      screen_info, compositor_deps_->CreateUkmRecorderFactory(),
      /*settings=*/nullptr);
  DCHECK(layer_tree_host_);
}

void RenderWidget::Close(std::unique_ptr<RenderWidget> widget) {
  // At the end of this method, |widget| which points to this is deleted.
  DCHECK_EQ(widget.get(), this);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(!closing_);

  closing_ = true;

  webwidget_->Close(compositor_deps_->GetCleanupTaskRunner());
  webwidget_ = nullptr;

  // |layer_tree_host_| is valid only when |webwidget_| is valid. Close may
  // use the WebWidgetClient while unloading the Frame so we clear this
  // after.
  layer_tree_host_ = nullptr;
}

blink::WebFrameWidget* RenderWidget::GetFrameWidget() const {
  // TODO(danakj): Remove this check and don't call this method for non-frames.
  if (!for_frame())
    return nullptr;
  return static_cast<blink::WebFrameWidget*>(webwidget_);
}

void RenderWidget::ConvertViewportToWindow(blink::WebRect* rect) {
  if (compositor_deps_->IsUseZoomForDSFEnabled()) {
    float reverse =
        1 / GetWebWidget()->GetOriginalScreenInfo().device_scale_factor;
    // TODO(oshima): We may need to allow pixel precision here as the the
    // anchor element can be placed at half pixel.
    gfx::Rect window_rect = gfx::ScaleToEnclosedRect(gfx::Rect(*rect), reverse);
    rect->x = window_rect.x();
    rect->y = window_rect.y();
    rect->width = window_rect.width();
    rect->height = window_rect.height();
  }
}

void RenderWidget::UpdateSelectionBounds() {
  GetWebWidget()->UpdateSelectionBounds();
}

}  // namespace content

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
#include "cc/trees/ukm_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_widget.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace content {

// RenderWidget ---------------------------------------------------------------

std::unique_ptr<RenderWidget> RenderWidget::CreateForFrame(
    CompositorDependencies* compositor_deps) {
  return std::make_unique<RenderWidget>(base::PassKey<RenderWidget>(),
                                        compositor_deps);
}

RenderWidget::RenderWidget(base::PassKey<RenderWidget>,
                           CompositorDependencies* compositor_deps)
    : compositor_deps_(compositor_deps) {
  DCHECK(RenderThread::IsMainThread());
  DCHECK(compositor_deps_);
}

RenderWidget::~RenderWidget() {
  DCHECK(!webwidget_) << "Leaking our WebWidget!";
  DCHECK(closing_)
      << " RenderWidget must be destroyed via RenderWidget::Close()";
}

void RenderWidget::InitForMainFrame(blink::WebFrameWidget* web_frame_widget,
                                    const blink::ScreenInfo& screen_info) {
  Initialize(web_frame_widget, screen_info);
}

void RenderWidget::InitForChildLocalRoot(
    blink::WebFrameWidget* web_frame_widget,
    const blink::ScreenInfo& screen_info) {
  Initialize(web_frame_widget, screen_info);
}

void RenderWidget::CloseForFrame(std::unique_ptr<RenderWidget> widget) {
  DCHECK_EQ(widget.get(), this);  // This method takes ownership of |this|.

  Close(std::move(widget));
}

void RenderWidget::Initialize(blink::WebWidget* web_widget,
                              const blink::ScreenInfo& screen_info) {
  DCHECK(web_widget);

  webwidget_ = web_widget;
  InitCompositing(screen_info);
}

///////////////////////////////////////////////////////////////////////////////
// WebWidgetClient

void RenderWidget::InitCompositing(const blink::ScreenInfo& screen_info) {
  TRACE_EVENT0("blink", "RenderWidget::InitializeLayerTreeView");

  webwidget_->InitializeCompositing(
      compositor_deps_->GetWebMainThreadScheduler(),
      compositor_deps_->GetTaskGraphRunner(), screen_info,
      compositor_deps_->CreateUkmRecorderFactory(),
      /*settings=*/nullptr);
}

void RenderWidget::Close(std::unique_ptr<RenderWidget> widget) {
  // At the end of this method, |widget| which points to this is deleted.
  DCHECK_EQ(widget.get(), this);
  DCHECK(RenderThread::IsMainThread());
  DCHECK(!closing_);

  closing_ = true;

  webwidget_->Close();
  webwidget_ = nullptr;
}

}  // namespace content

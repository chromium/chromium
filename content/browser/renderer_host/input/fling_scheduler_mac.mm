// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler_mac.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "ui/compositor/compositor.h"

namespace content {

FlingSchedulerMac::FlingSchedulerMac(RenderWidgetHostImpl* host)
    : FlingScheduler(host) {}
FlingSchedulerMac::~FlingSchedulerMac() = default;

ui::Compositor* FlingSchedulerMac::GetCompositor() {
  RenderWidgetHostViewBase* view = host_->GetView();
  if (!view)
    return nullptr;

  if (view->IsRenderWidgetHostViewChildFrame()) {
    view = view->GetRootView();
    if (!view)
      return nullptr;
  }

  RenderWidgetHostViewMac* mac_view =
      static_cast<RenderWidgetHostViewMac*>(view);
  if (mac_view->BrowserCompositor())
    return mac_view->BrowserCompositor()->GetCompositor();

  return nullptr;
}

}  // namespace content

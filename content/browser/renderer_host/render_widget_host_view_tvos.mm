// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_tvos.h"

#import <UIKit/UIKit.h>

#include "ui/accelerated_widget_mac/display_ca_layer_tree.h"

namespace content {

RenderWidgetHostViewTVOS::RenderWidgetHostViewTVOS(RenderWidgetHost* widget)
    : RenderWidgetHostViewIOS(widget) {
  display_tree_ =
      std::make_unique<ui::DisplayCALayerTree>([GetNativeView().Get() layer]);
}

RenderWidgetHostViewTVOS::~RenderWidgetHostViewTVOS() = default;

void RenderWidgetHostViewTVOS::UpdateCALayerTree(
    gfx::CALayerParams ca_layer_params) {
  CHECK(display_tree_, base::NotFatalUntil::M152);
  display_tree_->UpdateCALayerTree(std::move(ca_layer_params));
}

void RenderWidgetHostViewTVOS::OnGestureEvent(
    const ui::GestureEventData& gesture) {}

bool RenderWidgetHostViewTVOS::RequiresDoubleTapGestureEvents() const {
  return false;
}

}  // namespace content

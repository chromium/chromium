// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_H_

#include <memory>

#include "content/browser/renderer_host/render_widget_host_view_ios.h"

namespace ui {

class DisplayCALayerTree;

}  // namespace ui

namespace content {

class CONTENT_EXPORT RenderWidgetHostViewTVOS : public RenderWidgetHostViewIOS {
 public:
  RenderWidgetHostViewTVOS(RenderWidgetHost* widget);
  ~RenderWidgetHostViewTVOS() override;

  RenderWidgetHostViewTVOS(const RenderWidgetHostViewTVOS&) = delete;
  RenderWidgetHostViewTVOS& operator=(const RenderWidgetHostViewTVOS&) = delete;

  // ui::CALayerFrameSink overrides:
  void UpdateCALayerTree(const gfx::CALayerParams& ca_layer_params) override;

 private:
  std::unique_ptr<ui::DisplayCALayerTree> display_tree_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_H_

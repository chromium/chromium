// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink_provider.h"

@interface RenderWidgetUIView : CALayerFrameSinkProvider {
  base::WeakPtr<content::RenderWidgetHostViewIOS> _view;
}

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view;

- (void)updateView:(UIScrollView*)view;
- (void)removeView;
@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_

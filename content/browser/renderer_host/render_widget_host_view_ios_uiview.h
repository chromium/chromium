// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_H_

#import <BrowserEngineKit/BrowserEngineKit.h>
#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/renderer_host/render_widget_host_view_ios_uiview_textinput.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink_provider.h"
#include "ui/gfx/geometry/vector2d_f.h"

@interface RenderWidgetUIView : CALayerFrameSinkProvider <BETextInput> {
  base::WeakPtr<content::RenderWidgetHostViewIOS> _view;
  id<BETextInputDelegate> be_text_input_delegate_;
  BETextInteraction* text_interaction_;
  std::optional<gfx::Vector2dF> _viewOffsetDuringTouchSequence;
}

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view;

// TextInput state.
@property(nonatomic, strong) RenderWidgetUIViewTextInput* textInput;
- (BETextInteraction*)textInteraction;
- (void)updateView:(UIScrollView*)view;
- (void)removeView;
@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_H_

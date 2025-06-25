// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "ui/accelerated_widget_mac/ca_layer_frame_sink_provider.h"

@interface RenderWidgetUIView
    : CALayerFrameSinkProvider <UITextFieldDelegate,
                                UIGestureRecognizerDelegate> {
  base::WeakPtr<content::RenderWidgetHostViewIOS> _view;

  // Contrary to what Apple's documentation says, on tvOS calling
  // becomeFirstResponder on UIKeyInput does not show the on-screen keyboard,
  // but doing so on a UITextField instance does, so we use this text field to
  // show/hide the system keyboard and plumb its value to |_view| and Blink.
  UITextField* _textFieldForAllTextInput;
}

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view;

- (void)onUpdateTextInputState:(const ui::mojom::TextInputState&)state
                    withBounds:(CGRect)bounds;

- (void)updateView:(UIScrollView*)view;
- (void)removeView;
@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_TVOS_UIVIEW_H_

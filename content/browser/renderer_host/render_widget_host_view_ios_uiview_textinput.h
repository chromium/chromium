// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_TEXTINPUT_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_TEXTINPUT_H_

#import <UIKit/UIKit.h>

#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_ios.h"
#include "content/browser/renderer_host/render_widget_host_view_ios_uiview_textinput.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-forward.h"
#include "ui/gfx/geometry/vector2d_f.h"

@interface RenderWidgetUIViewTextInput : UIView <UITextInput> {
  base::WeakPtr<content::RenderWidgetHostViewIOS> _view;
}

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view;
- (void)onUpdateTextInputState:(const ui::mojom::TextInputState&)state
                    withBounds:(CGRect)bounds;
- (void)showKeyboard:(bool)has_text withBounds:(CGRect)bounds;
- (void)hideKeyboard;

@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_IOS_UIVIEW_TEXTINPUT_H_

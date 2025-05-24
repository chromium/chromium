// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_tvos_uiview.h"

#include "base/notimplemented.h"

static void* kObservingContext = &kObservingContext;

@implementation RenderWidgetUIView

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view {
  self = [self init];
  if (self) {
    _view = view;
    // TODO(crbug.com/411452047): Need to support text input.
    TVOS_NOT_YET_IMPLEMENTED();
  }
  return self;
}

- (void)layoutSubviews {
  CHECK(_view);
  [super layoutSubviews];
  _view->UpdateScreenInfo();

  // TODO(dtapuska): This isn't correct, we need to figure out when the window
  // gains/loses focus.
  _view->SetActive(true);
}

- (ui::CALayerFrameSink*)frameSink {
  return _view.get();
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  CHECK(_view);
  if (context == kObservingContext) {
    _view->ContentInsetChanged();
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}

- (void)removeView {
  UIScrollView* view = (UIScrollView*)[self superview];
  [view removeObserver:self
            forKeyPath:NSStringFromSelector(@selector(contentInset))];
  [self removeFromSuperview];
}

- (void)updateView:(UIScrollView*)view {
  [view addSubview:self];
  view.scrollEnabled = NO;
  // Remove all existing gestureRecognizers since the header might be reused.
  for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
    [view removeGestureRecognizer:recognizer];
  }
  [view addObserver:self
         forKeyPath:NSStringFromSelector(@selector(contentInset))
            options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
            context:kObservingContext];
}

@end

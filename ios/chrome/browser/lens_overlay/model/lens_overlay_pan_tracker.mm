// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"

@implementation LensOverlayPanTracker {
  __weak UIView* _view;
  UIPanGestureRecognizer* _panRecognizer;
}

- (instancetype)initWithView:(UIView*)view {
  self = [super init];
  if (self) {
    _view = view;
  }

  return self;
}

- (void)startTracking {
  if (!_view || _panRecognizer) {
    return;
  }

  _panRecognizer =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  _panRecognizer.delegate = self;
  _panRecognizer.cancelsTouchesInView = NO;
  [_view addGestureRecognizer:_panRecognizer];
}

- (void)stopTracking {
  if (!_view || !_panRecognizer) {
    return;
  }

  [_view removeGestureRecognizer:_panRecognizer];
  _panRecognizer = nil;
}

- (void)handlePan:(UIPanGestureRecognizer*)recognizer {
  BOOL isStarting = recognizer.state == UIGestureRecognizerStateBegan;
  if (isStarting) {
    _isPanning = YES;
    [_delegate onPanGestureStarted:self];
    return;
  }

  BOOL isEnding = recognizer.state == UIGestureRecognizerStateEnded;
  BOOL isCancelled = recognizer.state == UIGestureRecognizerStateCancelled;

  if (isEnding || isCancelled) {
    _isPanning = NO;
    [_delegate onPanGestureEnded:self];
    return;
  }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

@end

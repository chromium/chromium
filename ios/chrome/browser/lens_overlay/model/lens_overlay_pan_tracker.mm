// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"

@implementation LensOverlayPanTracker {
  // The view on which to start recognizing panning.
  __weak UIView* _view;

  // The gesture recognizer used for tracking.
  UIPanGestureRecognizer* _panRecognizer;
}

- (instancetype)initWithView:(UIView*)view {
  self = [super init];
  if (self) {
    _view = view;
    _cancelsTouchesInView = NO;
  }

  return self;
}

- (void)setCancelsTouchesInView:(BOOL)cancelsTouchesInView {
  _cancelsTouchesInView = cancelsTouchesInView;
  _panRecognizer.cancelsTouchesInView = cancelsTouchesInView;
}

- (void)startTracking {
  if (!_view || _panRecognizer) {
    return;
  }

  _panRecognizer =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  _panRecognizer.delegate = self;
  _panRecognizer.cancelsTouchesInView = _cancelsTouchesInView;
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
    if ([_delegate respondsToSelector:@selector
                   (lensOverlayPanTrackerDidBeginPanGesture:)]) {
      [_delegate lensOverlayPanTrackerDidBeginPanGesture:self];
    }
    return;
  }

  BOOL isEnding = recognizer.state == UIGestureRecognizerStateEnded;
  BOOL isCancelled = recognizer.state == UIGestureRecognizerStateCancelled;

  CGPoint translation = [recognizer translationInView:_view];
  CGPoint velocity = [recognizer velocityInView:_view];

  if (isEnding || isCancelled) {
    _isPanning = NO;
    if ([_delegate respondsToSelector:@selector(lensOverlayPanTracker:
                                          didEndPanGestureWithVelocity:)]) {
      [_delegate lensOverlayPanTracker:self
          didEndPanGestureWithVelocity:velocity];
    }

    return;
  }

  if ([_delegate respondsToSelector:@selector
                 (lensOverlayPanTracker:didPanWithTranslation:velocity:)]) {
    [_delegate lensOverlayPanTracker:self
               didPanWithTranslation:translation
                            velocity:velocity];
  }
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if ([_delegate respondsToSelector:@selector
                 (lensOverlayPanTracker:
                     shouldRecognizeSimultaneouslyWithGestureRecognizer:)]) {
    return [_delegate lensOverlayPanTracker:self
        shouldRecognizeSimultaneouslyWithGestureRecognizer:
            otherGestureRecognizer];
  }
  return YES;
}

@end

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_touch_tracking_recognizer.h"

@interface CRWTouchTrackingRecognizer () <UIGestureRecognizerDelegate>
@end

@implementation CRWTouchTrackingRecognizer

- (id)initWithTouchTrackingDelegate:
    (id<CRWTouchTrackingDelegate>)touchTrackingDelegate {
  if ((self = [super init])) {
    _touchTrackingDelegate = touchTrackingDelegate;
    self.delegate = self;
  }
  return self;
}

#pragma mark -
#pragma mark UIGestureRecognizer Methods

- (void)reset {
  [super reset];
}

- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesBegan:touches withEvent:event];
  [self.touchTrackingDelegate touched:YES];
}

- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesEnded:touches withEvent:event];
  self.state = UIGestureRecognizerStateFailed;
  [self.touchTrackingDelegate touched:NO];
}

- (void)touchesCancelled:(NSSet*)touches withEvent:(UIEvent*)event {
  [super touchesCancelled:touches withEvent:event];
  self.state = UIGestureRecognizerStateCancelled;
  [self.touchTrackingDelegate touched:NO];
}

#pragma mark -
#pragma mark UIGestureRecognizerDelegate Method

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

@end

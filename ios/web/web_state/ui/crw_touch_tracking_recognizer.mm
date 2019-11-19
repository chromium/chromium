// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_touch_tracking_recognizer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

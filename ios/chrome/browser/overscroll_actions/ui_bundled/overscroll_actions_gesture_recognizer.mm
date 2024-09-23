// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overscroll_actions/ui_bundled/overscroll_actions_gesture_recognizer.h"

#import <UIKit/UIGestureRecognizerSubclass.h>

#import "base/check.h"

@interface OverscrollActionsGestureRecognizer () {
  __weak id _target;
  SEL _action;
}
@end

@implementation OverscrollActionsGestureRecognizer

- (instancetype)initWithTarget:(id)target action:(SEL)action {
  self = [super initWithTarget:target action:action];
  if (self) {
    _target = target;
    _action = action;
  }
  return self;
}

- (void)reset {
  [super reset];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [_target performSelector:_action withObject:self];
#pragma clang diagnostic pop
}

- (void)removeTarget:(id)target action:(SEL)action {
  DCHECK(target != _target);
  [super removeTarget:target action:action];
}

@end

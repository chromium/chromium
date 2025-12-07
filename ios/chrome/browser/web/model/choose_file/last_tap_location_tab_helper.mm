// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/last_tap_location_tab_helper.h"

#import <UIKit/UIKit.h>

#import "base/functional/callback.h"
#import "ios/web/public/web_state.h"

// A controller that recognizes tap gestures and calls a callback.
@interface LastTapLocationController : NSObject <UIGestureRecognizerDelegate>
- (instancetype)initWithCallback:
    (base::RepeatingCallback<void(UITapGestureRecognizer*)>)callback;
@end

@implementation LastTapLocationController {
  base::RepeatingCallback<void(UITapGestureRecognizer*)> _callback;
}

- (instancetype)initWithCallback:
    (base::RepeatingCallback<void(UITapGestureRecognizer*)>)callback {
  self = [super init];
  if (self) {
    CHECK(callback);
    _callback = std::move(callback);
  }
  return self;
}

- (void)handleTap:(UITapGestureRecognizer*)sender {
  _callback.Run(sender);
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

@end

LastTapLocationTabHelper::LastTapLocationTabHelper(web::WebState* web_state) {
  observation_.Observe(web_state);
  if (web_state->IsVisible()) {
    WasShown(web_state);
  }
}

LastTapLocationTabHelper::~LastTapLocationTabHelper() {
  if (tap_gesture_recognizer_) {
    [tap_gesture_recognizer_.view
        removeGestureRecognizer:tap_gesture_recognizer_];
  }
}

CGPoint LastTapLocationTabHelper::GetLastTapPoint() const {
  return last_tap_point_;
}

void LastTapLocationTabHelper::HandleTap(UITapGestureRecognizer* sender) {
  last_tap_point_ = [sender locationInView:sender.view];
}

#pragma mark - web::WebStateObserver

void LastTapLocationTabHelper::WasShown(web::WebState* web_state) {
  if (tap_gesture_recognizer_) {
    return;
  }
  tap_gesture_target_ = [[LastTapLocationController alloc]
      initWithCallback:base::BindRepeating(&LastTapLocationTabHelper::HandleTap,
                                           weak_ptr_factory_.GetWeakPtr())];
  tap_gesture_recognizer_ =
      [[UITapGestureRecognizer alloc] initWithTarget:tap_gesture_target_
                                              action:@selector(handleTap:)];
  tap_gesture_recognizer_.delegate = tap_gesture_target_;
  tap_gesture_recognizer_.cancelsTouchesInView = NO;
  [web_state->GetView() addGestureRecognizer:tap_gesture_recognizer_];
}

void LastTapLocationTabHelper::WasHidden(web::WebState* web_state) {
  if (tap_gesture_recognizer_) {
    [tap_gesture_recognizer_.view
        removeGestureRecognizer:tap_gesture_recognizer_];
    tap_gesture_recognizer_ = nil;
    tap_gesture_target_ = nil;
  }
}

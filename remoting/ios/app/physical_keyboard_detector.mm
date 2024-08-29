// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/physical_keyboard_detector.h"

// A view to do measurement to figure out whether a physical keyboard is
// presented.
//
// The logic is hacky since iOS doesn't provide any API that tells you whether
// a physical keyboard is being used. We infer that in these steps:
//
//   1. Add a hidden text field (this view) to the view to be detected and
//      register the keyboardWillShow notification.
//   2. Make the text field first responder.
//   3. keyboardWillShow will get called. The keyboard's end frame will go
//      offscreen if the physical keyboard is presented.
//   4. Pass that information to the callback and remove the hidden text field.
//      The view will not flicker as long as we immediately remove the text field
//      in keyboardWillShow.
//
// Unfortunately there is no easy way to know immediately when the user connects
// or disconnects the keyboard.
@interface PhysicalKeyboardDetectorView : UITextField {
  void (^_callback)(BOOL);
}

- (void)detectOnView:(UIView*)view callback:(void (^)(BOOL))callback;

@end

@implementation PhysicalKeyboardDetectorView

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    self.hidden = YES;

    // This is to force keyboardWillShow to always be called.
    self.inputAccessoryView = [[UIView alloc] initWithFrame:CGRectZero];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)detectOnView:(UIView*)view callback:(void (^)(BOOL))callback {
  _callback = callback;
  [view addSubview:self];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];
  [self becomeFirstResponder];
}

#pragma mark - Private

- (void)keyboardWillShow:(NSNotification*)notification {
  if (!self.isFirstResponder) {
    // Don't handle the notification if it is not for the detector view.
    return;
  }

  CGRect keyboardFrame = [(NSValue*)[notification.userInfo
      objectForKey:UIKeyboardFrameEndUserInfoKey] CGRectValue];

  // iPad will still show the toolbar at the top of the soft keyboard even if
  // the physical keyboard is presented, so the safer check is to see if the
  // bottom of the keyboard goes under the screen.
  int keyboardBottom = keyboardFrame.origin.y + keyboardFrame.size.height;
  BOOL isKeyboardOffScreen =
      keyboardBottom > UIScreen.mainScreen.bounds.size.height;
  [self resignFirstResponder];
  [self removeFromSuperview];
  _callback(isKeyboardOffScreen);
}

@end

#pragma mark - PhysicalKeyboardDetector

@implementation PhysicalKeyboardDetector

+ (void)detectOnView:(UIView*)view callback:(void (^)(BOOL))callback {
  PhysicalKeyboardDetectorView* detectorView =
      [[PhysicalKeyboardDetectorView alloc] initWithFrame:CGRectZero];
  [detectorView detectOnView:view callback:callback];
}

@end

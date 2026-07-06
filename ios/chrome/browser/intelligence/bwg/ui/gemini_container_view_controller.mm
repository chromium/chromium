// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_presentation_context.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation GeminiContainerViewController {
  // The child view controller wrapping the actual Gemini UI provided by the
  // provider.
  UIViewController* _geminiViewController;
}

- (instancetype)initWithGeminiViewController:
    (UIViewController*)geminiViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _geminiViewController = geminiViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillShow:)
                        name:UIKeyboardWillShowNotification
                      object:nil];

  if (_geminiViewController) {
    [self addChildViewController:_geminiViewController];
    _geminiViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:_geminiViewController.view];
    AddSameConstraints(_geminiViewController.view, self.view);
    [_geminiViewController didMoveToParentViewController:self];
  }
}

#pragma mark - Private

// Called right before the keyboard is shown.
- (void)keyboardWillShow:(NSNotification*)notification {
  // Only proceed if the keyboard appeared because the view inside this
  // container or its subviews are the first responder.
  if (!GetFirstResponderSubview(self.view)) {
    return;
  }

  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);
  [self.delegate geminiContainerViewController:self
                   didShowKeyboardWithDuration:duration
                                         curve:curve];
}

@end

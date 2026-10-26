// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/eg_tests/inttest/composebox_inttest_coordinator.h"

#import "ios/chrome/browser/autocomplete/model/autocomplete_browser_agent.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_coordinator.h"
#import "ios/chrome/browser/composebox/public/composebox_entrypoint.h"
#import "ios/chrome/browser/composebox/public/composebox_focus_params.h"

@implementation ComposeboxInttestCoordinator {
  ComposeboxCoordinator* _composeboxCoordinator;
  // Opaque view covering the area of the software keyboard. See
  // `-keyboardDidShow:`.
  UIView* _keyboardCoverView;
}

- (void)start {
  // Initialize dependencies.
  AutocompleteBrowserAgent::CreateForBrowser(self.browser);

  // Observe the keyboard before starting the composebox, as focusing the
  // composebox presents the keyboard.
  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];
  [notificationCenter addObserver:self
                         selector:@selector(keyboardDidShow:)
                             name:UIKeyboardDidShowNotification
                           object:nil];
  [notificationCenter addObserver:self
                         selector:@selector(keyboardWillHide:)
                             name:UIKeyboardWillHideNotification
                           object:nil];

  // Start coordinator.
  ComposeboxFocusParams* params = [[ComposeboxFocusParams alloc]
      initWithEntrypoint:ComposeboxEntrypoint::kOther];

  _composeboxCoordinator = [[ComposeboxCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                     focusParams:params
         composeboxAnimationBase:nil];
  [_composeboxCoordinator start];
}

- (void)stop {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self removeKeyboardCoverView];

  // Stop coordinator.
  [_composeboxCoordinator stop];
  _composeboxCoordinator = nil;

  // Cleanup dependencies.
  AutocompleteBrowserAgent* autocompleteBrowserAgent =
      AutocompleteBrowserAgent::FromBrowser(self.browser);
  if (autocompleteBrowserAgent) {
    // Cleanup test autocomplete that might have been added.
    autocompleteBrowserAgent->RemoveServices();
  }
}

#pragma mark - Keyboard notifications

// The software keyboard is hosted in a separate window that EarlGrey does not
// render when checking visibility, so views covered by the keyboard are
// considered visible. Cover the keyboard area with an opaque view in the
// window of the base view controller so that EarlGrey visibility checks match
// what the user sees.
- (void)keyboardDidShow:(NSNotification*)notification {
  UIWindow* window = self.baseViewController.view.window;
  if (!window) {
    return;
  }
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  id<UICoordinateSpace> screenCoordinateSpace =
      ((UIScreen*)notification.object).coordinateSpace;
  CGRect keyboardFrameInWindow = CGRectIntersection(
      [screenCoordinateSpace convertRect:keyboardFrame
                       toCoordinateSpace:window.coordinateSpace],
      window.bounds);

  if (!_keyboardCoverView) {
    _keyboardCoverView = [[UIView alloc] init];
    _keyboardCoverView.backgroundColor = [UIColor blackColor];
    _keyboardCoverView.userInteractionEnabled = NO;
  }
  _keyboardCoverView.frame = keyboardFrameInWindow;
  // Add the cover above everything in the window, including the composebox
  // which is presented over the base view controller.
  [window addSubview:_keyboardCoverView];
}

- (void)keyboardWillHide:(NSNotification*)notification {
  [self removeKeyboardCoverView];
}

// Removes the opaque view covering the keyboard area.
- (void)removeKeyboardCoverView {
  [_keyboardCoverView removeFromSuperview];
  _keyboardCoverView = nil;
}

@end

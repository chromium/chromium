// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view_delegate.h"

@interface SnackbarCoordinator () <SnackbarViewDelegate>
@end

@implementation SnackbarCoordinator {
  __weak id<SnackbarCoordinatorDelegate> _delegate;
  SnackbarView* _snackbarView;
  ChromeOverlayWindow* _overlay_window;
  // Flag to prevent dismissal logic from running multiple times from concurrent
  // events (e.g., user tap and timer firing simultaneously).
  BOOL _isDismissing;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  delegate:(id<SnackbarCoordinatorDelegate>)
                                               delegate {
  DCHECK(delegate);

  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  DCHECK(self.browser);

  UIWindow* window = self.browser->GetSceneState().window;
  if ([window isKindOfClass:[ChromeOverlayWindow class]]) {
    _overlay_window = base::apple::ObjCCastStrict<ChromeOverlayWindow>(window);
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(SnackbarCommands)];
}

- (void)stop {
  DCHECK(self.browser);
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];
  [self dismissAllSnackbars];
}

#pragma mark - SnackbarCommands

- (void)showSnackbarMessage:(SnackbarMessage*)message {
  CGFloat offset =
      [_delegate snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                                      forceBrowserToolbar:NO];
  [self showSnackbarMessage:message bottomOffset:offset];
}

- (void)showSnackbarMessageOverBrowserToolbar:(SnackbarMessage*)message {
  CGFloat offset =
      [_delegate snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                                      forceBrowserToolbar:YES];
  [self showSnackbarMessage:message bottomOffset:offset];
}

- (void)showSnackbarMessage:(SnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
  TriggerHapticFeedbackForNotification(type);
  [self showSnackbarMessage:message];
}

- (void)showSnackbarMessage:(SnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
  [self presentSnackbar:message withBottomOffset:offset];
}

- (void)showSnackbarMessageAfterDismissingKeyboard:(SnackbarMessage*)message {
  // Dismiss the keybord if present.
  [self.browser->GetSceneState().window endEditing:YES];

  [self showSnackbarMessage:message];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageText];
  if (buttonText) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = messageAction;
    action.title = buttonText;
    action.accessibilityLabel = buttonText;
    message.action = action;
  }
  message.completionHandler = completionAction;

  [self showSnackbarMessage:message];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
        buttonAccessibilityHint:(NSString*)buttonAccessibilityHint
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:messageText];
  if (buttonText) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = messageAction;
    action.title = buttonText;
    action.accessibilityHint = buttonAccessibilityHint;
    message.action = action;
  }
  message.completionHandler = completionAction;

  [self showSnackbarMessage:message];
}

- (void)dismissAllSnackbars {
  [self dismissSnackbar:_snackbarView animated:NO];
}

- (void)dismissSnackbar:(SnackbarView*)snackbarView animated:(BOOL)animated {
  if (!snackbarView || snackbarView != _snackbarView) {
    return;
  }

  // A dismissal can be triggered by the timer and by a user tap concurrently.
  // This flag prevents the dismissal logic from running more than once.
  if (_isDismissing) {
    return;
  }
  _isDismissing = YES;

  if (_snackbarView.message.completionHandler) {
    _snackbarView.message.completionHandler(NO);
  }

  __weak __typeof(self) weakSelf = self;
  [_snackbarView dismissAnimated:animated
                      completion:^{
                        [weakSelf removeSnackbarView];
                      }];
}

#pragma mark - SnackbarViewDelegate

- (void)snackbarViewDidTapActionButton:(SnackbarView*)snackbarView {
  [self dismissSnackbar:snackbarView animated:YES];
}

- (void)snackbarViewDidRequestDismissal:(SnackbarView*)snackbarView
                               animated:(BOOL)animated {
  [self dismissSnackbar:snackbarView animated:animated];
}

#pragma mark - Private

// Dismisses any currently visible snackbar, then creates, configures and
// presents a new `SnackbarView`.
- (void)presentSnackbar:(SnackbarMessage*)message
       withBottomOffset:(CGFloat)offset {
  // If a snackbar is already showing, dismiss it before showing the new one.
  if (_snackbarView) {
    [self dismissAllSnackbars];
  }
  _isDismissing = NO;

  // Create and configure the new snackbar view.
  _snackbarView = [[SnackbarView alloc] initWithMessage:message];
  _snackbarView.delegate = self;
  _snackbarView.bottomOffset = offset;

  // Add the snackbar to the window and present it.
  [_overlay_window activateOverlay:_snackbarView withLevel:UIWindowLevelNormal];
  [_snackbarView
      presentAnimated:YES
           completion:^{
               // The view will now schedule its own dismissal and call
               // the delegate when it's time.
           }];
}

// Removes the snackbar view from the hierarchy and nils out the ivar.
- (void)removeSnackbarView {
  if (!_snackbarView) {
    return;
  }
  [_overlay_window deactivateOverlay:_snackbarView];
  _snackbarView = nil;
  _isDismissing = NO;
}

@end

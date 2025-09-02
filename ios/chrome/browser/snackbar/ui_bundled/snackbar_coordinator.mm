// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_coordinator.h"

#import <MaterialComponents/MaterialOverlayWindow.h>

#import "base/apple/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snackbar/public/snackbar_message.h"
#import "ios/chrome/browser/snackbar/public/snackbar_message_action.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view_delegate.h"

namespace {
// The amount of time after a snackbar is presented, during which it will
// retain a11y focus so that VoiceOver is not interrupted by a modal dismissal
// transition.
const double kRetainA11yFocusSeconds = 0.75;
// The duration of the snackbar fade in/out animation.
const NSTimeInterval kSnackbarAnimationDuration = 0.3;
}  // namespace

@interface SnackbarCoordinator () <SnackbarViewDelegate>
@end

@implementation SnackbarCoordinator {
  __weak id<SnackbarCoordinatorDelegate> _delegate;
  SnackbarView* _snackbarView;
  MDCOverlayWindow* _overlay_window;
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
  if ([window isKindOfClass:[MDCOverlayWindow class]]) {
    _overlay_window = base::apple::ObjCCastStrict<MDCOverlayWindow>(window);
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

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
  SnackbarMessage* snackbarMessage =
      [[SnackbarMessage alloc] initWithMDCSnackbarMessage:message];
  [self showCustomSnackbarMessage:snackbarMessage];
}

- (void)showSnackbarMessageOverBrowserToolbar:(MDCSnackbarMessage*)message {
  SnackbarMessage* snackbarMessage =
      [[SnackbarMessage alloc] initWithMDCSnackbarMessage:message];
  [self showCustomSnackbarMessageOverBrowserToolbar:snackbarMessage];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
  SnackbarMessage* snackbarMessage =
      [[SnackbarMessage alloc] initWithMDCSnackbarMessage:message];
  [self showCustomSnackbarMessage:snackbarMessage withHapticType:type];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
  SnackbarMessage* snackbarMessage =
      [[SnackbarMessage alloc] initWithMDCSnackbarMessage:message];
  [self showCustomSnackbarMessage:snackbarMessage bottomOffset:offset];
}

- (void)showCustomSnackbarMessage:(SnackbarMessage*)message {
  CGFloat offset =
      [_delegate snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                                      forceBrowserToolbar:NO];
  [self showCustomSnackbarMessage:message bottomOffset:offset];
}

- (void)showCustomSnackbarMessageOverBrowserToolbar:(SnackbarMessage*)message {
  CGFloat offset =
      [_delegate snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                                      forceBrowserToolbar:YES];
  [self showCustomSnackbarMessage:message bottomOffset:offset];
}

- (void)showCustomSnackbarMessage:(SnackbarMessage*)message
                   withHapticType:(UINotificationFeedbackType)type {
  TriggerHapticFeedbackForNotification(type);
  [self showCustomSnackbarMessage:message];
}

- (void)showCustomSnackbarMessage:(SnackbarMessage*)message
                     bottomOffset:(CGFloat)offset {
  [self presentSnackbar:message withBottomOffset:offset];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  SnackbarMessage* message = CreateCustomSnackbarMessage(messageText);
  if (buttonText) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = messageAction;
    action.title = buttonText;
    action.accessibilityLabel = buttonText;
    message.action = action;
  }
  message.completionHandler = completionAction;

  [self showCustomSnackbarMessage:message];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
        buttonAccessibilityHint:(NSString*)buttonAccessibilityHint
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  SnackbarMessage* message = CreateCustomSnackbarMessage(messageText);
  if (buttonText) {
    SnackbarMessageAction* action = [[SnackbarMessageAction alloc] init];
    action.handler = messageAction;
    action.title = buttonText;
    action.accessibilityHint = buttonAccessibilityHint;
    message.action = action;
  }
  message.completionHandler = completionAction;

  [self showCustomSnackbarMessage:message];
}

- (void)dismissAllSnackbars {
  [self dismissSnackbar:_snackbarView animated:NO];
}

- (void)dismissSnackbar:(SnackbarView*)snackbarView animated:(BOOL)animated {
  if (!snackbarView || snackbarView != _snackbarView) {
    return;
  }

  if (_snackbarView.message.completionHandler) {
    _snackbarView.message.completionHandler(NO);
  }

  BOOL shouldAnimate = animated && !UIAccessibilityIsReduceMotionEnabled();
  if (shouldAnimate) {
    [UIView animateWithDuration:kSnackbarAnimationDuration
        animations:^{
          [self updateSnackbarAlpha:0.0];
        }
        completion:^(BOOL finished) {
          [self removeSnackbarView];
        }];
  } else {
    [self removeSnackbarView];
  }
}

#pragma mark - SnackbarViewDelegate

- (void)snackbarViewDidTapActionButton:(SnackbarView*)snackbarView {
  [self dismissSnackbar:snackbarView animated:YES];
}

- (void)snackbarViewWasTapped:(SnackbarView*)snackbarView {
  [self dismissSnackbar:snackbarView animated:YES];
}

#pragma mark - Private

// Dismisses any currently visible snackbar, then creates, configures and
// presents a new `SnackbarView`.
- (void)presentSnackbar:(SnackbarMessage*)message
       withBottomOffset:(CGFloat)offset {
  if (_snackbarView) {
    [self dismissAllSnackbars];
  }

  _snackbarView = [[SnackbarView alloc] initWithMessage:message];
  _snackbarView.delegate = self;
  _snackbarView.bottomOffset = offset;

  [_overlay_window activateOverlay:_snackbarView withLevel:UIWindowLevelNormal];

  if (!UIAccessibilityIsReduceMotionEnabled()) {
    [self updateSnackbarAlpha:0.0];
    [UIView animateWithDuration:kSnackbarAnimationDuration
                     animations:^{
                       [self updateSnackbarAlpha:1.0];
                     }];
  }

  if (UIAccessibilityIsVoiceOverRunning()) {
    [self retainAccessibilityFocusOnView:_snackbarView.accessibilityFocusView
                                 seconds:kRetainA11yFocusSeconds];
    // Don't dismiss the snackbar if VoiceOver is running.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  __weak SnackbarView* weakSnackbarView = _snackbarView;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               (int64_t)(message.duration * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [weakSelf dismissSnackbar:weakSnackbarView animated:YES];
                 });
}

// Updates the alpha of the snackbar view.
- (void)updateSnackbarAlpha:(CGFloat)alpha {
  _snackbarView.alpha = alpha;
}

// Removes the snackbar view from the hierarchy and nils out the ivar.
- (void)removeSnackbarView {
  [_overlay_window deactivateOverlay:_snackbarView];
  _snackbarView = nil;
}

#pragma mark - Private

// Forces `view` to retain the accessibility focus for `seconds`. If
// another view becomes focused, the focus is forced back to `view`.
- (void)retainAccessibilityFocusOnView:(UIView*)view seconds:(double)seconds {
  __weak UIView* weakView = view;
  auto retainFocus = ^(NSNotification* notification) {
    id focusedElement = notification.userInfo[UIAccessibilityFocusedElementKey];
    if (weakView && focusedElement != weakView) {
      UIAccessibilityPostNotification(UIAccessibilityLayoutChangedNotification,
                                      weakView);
    }
  };

  // Observe accessibility focus changes.
  id observer = [[NSNotificationCenter defaultCenter]
      addObserverForName:UIAccessibilityElementFocusedNotification
                  object:nil
                   queue:nil
              usingBlock:retainFocus];

  // Stop observing after `seconds`.
  dispatch_time_t time =
      dispatch_time(DISPATCH_TIME_NOW, seconds * NSEC_PER_SEC);
  dispatch_after(time, dispatch_get_main_queue(), ^{
    [[NSNotificationCenter defaultCenter] removeObserver:observer];
  });
}

@end

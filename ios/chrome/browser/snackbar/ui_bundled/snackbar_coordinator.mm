// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/public/provider/chrome/browser/material/material_branding_api.h"

namespace {
// The amount of time after a snackbar is presented, during which it will
// retain a11y focus so that VoiceOver is not interrupted by a modal dismissal
// transition.
const double kRetainA11yFocusSeconds = 0.75;
}  // namespace

@interface SnackbarCoordinator () <MDCSnackbarManagerDelegate>

@property(nonatomic, weak) id<SnackbarCoordinatorDelegate> delegate;

@end

@implementation SnackbarCoordinator

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

  MDCSnackbarManager* manager = [MDCSnackbarManager defaultManager];
  manager.delegate = self;

  ios::provider::ApplyBrandingToSnackbarManager(manager);

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(SnackbarCommands)];
}

- (void)stop {
  DCHECK(self.browser);
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];
}

#pragma mark - SnackbarCommands

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
  CGFloat offset = [self.delegate
      snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                           forceBrowserToolbar:NO];
  [self showSnackbarMessage:message bottomOffset:offset];
}

- (void)showSnackbarMessageOverBrowserToolbar:(MDCSnackbarMessage*)message {
  CGFloat offset = [self.delegate
      snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self
                                           forceBrowserToolbar:YES];
  [self showSnackbarMessage:message bottomOffset:offset];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
  TriggerHapticFeedbackForNotification(type);
  [self showSnackbarMessage:message];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
  message.usesLegacyDismissalBehavior = YES;
  [[MDCSnackbarManager defaultManager]
      setPresentationHostView:self.baseViewController.view.window];
  [[MDCSnackbarManager defaultManager] setBottomOffset:offset];
  [[MDCSnackbarManager defaultManager] showMessage:message];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
  if (buttonText) {
    MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
    action.handler = messageAction;
    action.title = buttonText;
    action.accessibilityLabel = buttonText;
    message.action = action;
  }
  message.completionHandler = completionAction;

  [self showSnackbarMessage:message];
}

- (void)dismissAllSnackbars {
  [[MDCSnackbarManager defaultManager]
      dismissAndCallCompletionBlocksWithCategory:nil];
}

#pragma mark - MDCSnackbarManagerDelegate

- (void)snackbarManager:(MDCSnackbarManager*)snackbarManager
    willPresentSnackbarWithMessageView:(MDCSnackbarMessageView*)messageView {
  ios::provider::ApplyBrandingToSnackbarMessageView(messageView);

  if (UIAccessibilityIsVoiceOverRunning()) {
    // This snackbar may be presented right before a modal is dismissed, which
    // would cause the a11y focus to return to the top of the view that
    // presented it, interrupting VoiceOver as it is reading the snackbar's
    // message. To prevent that, focus changes will be observed for a period of
    // time so that the snackbar can retain the focus by brute force.
    [self retainAccessibilityFocusOnView:messageView
                                 seconds:kRetainA11yFocusSeconds];
  }
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

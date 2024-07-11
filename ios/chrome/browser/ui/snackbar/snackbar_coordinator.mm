// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/public/provider/chrome/browser/material/material_branding_api.h"

// Allow access to `usesLegacyDismissalBehavior` since the autoroller to update
// the header is broken.
@interface MDCSnackbarMessage (UsesLegacyDismissalBehavior)
@property(nonatomic) BOOL usesLegacyDismissalBehavior;
@end

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
  if ([message respondsToSelector:@selector(setUsesLegacyDismissalBehavior:)]) {
    message.usesLegacyDismissalBehavior = YES;
  }

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

#pragma mark - MDCSnackbarManagerDelegate

- (void)snackbarManager:(MDCSnackbarManager*)snackbarManager
    willPresentSnackbarWithMessageView:(MDCSnackbarMessageView*)messageView {
  ios::provider::ApplyBrandingToSnackbarMessageView(messageView);
}

@end

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snackbar/ui_bundled/snackbar_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/field_trial_params.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_window.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view.h"
#import "ios/chrome/browser/snackbar/ui_bundled/ui/snackbar_view_delegate.h"

@interface SnackbarCoordinator () <SnackbarCommands, SnackbarViewDelegate>
@end

@implementation SnackbarCoordinator {
  __weak id<SnackbarCoordinatorDelegate> _delegate;
  SnackbarView* _snackbarView;
  ChromeOverlayWindow* _overlay_window;
  __weak id<BWGCommands> _geminiHandler;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  delegate:(id<SnackbarCoordinatorDelegate>)
                                               delegate {
  DCHECK(delegate);

  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _delegate = delegate;
    if (IsGeminiCopresenceEnabled()) {
      _geminiHandler =
          HandlerForProtocol(self.browser->GetCommandDispatcher(), BWGCommands);
    }
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
  _geminiHandler = nil;
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

  if (_snackbarView.message.completionHandler) {
    _snackbarView.message.completionHandler(NO);
  }

  __weak id<BWGCommands> weakGeminiHandler = _geminiHandler;
  [_snackbarView
      dismissAnimated:animated
           completion:^() {
             [weakGeminiHandler
                 updateFloatyVisibilityIfEligibleAnimated:NO
                                               fromSource:
                                                   gemini::FloatyUpdateSource::
                                                       Snackbar];
           }];
  [_overlay_window deactivateOverlay:_snackbarView];
  _snackbarView.delegate = nil;
  _snackbarView = nil;
}

#pragma mark - SnackbarViewDelegate

- (void)snackbarViewDidTapActionButton:(SnackbarView*)snackbarView {
  CHECK_EQ(snackbarView, _snackbarView, base::NotFatalUntil::M152);
  [self dismissSnackbar:snackbarView animated:YES];
}

- (void)snackbarViewDidRequestDismissal:(SnackbarView*)snackbarView {
  CHECK_EQ(snackbarView, _snackbarView, base::NotFatalUntil::M152);
  [self dismissSnackbar:snackbarView animated:YES];
}

#pragma mark - Private

// Dismisses any currently visible snackbar, then creates, configures and
// presents a new `SnackbarView`.
- (void)presentSnackbar:(SnackbarMessage*)message
       withBottomOffset:(CGFloat)offset {
  CHECK(message, base::NotFatalUntil::M147);
  // If a snackbar is already showing, dismiss it before showing the new one.
  if (_snackbarView) {
    [self dismissAllSnackbars];
  }

  // Create and configure the new snackbar view.
  _snackbarView = [[SnackbarView alloc] initWithMessage:message];
  _snackbarView.delegate = self;
  _snackbarView.bottomOffset = offset;

  // Add the snackbar to the window and present it.
  [_geminiHandler
      hideFloatyIfInvokedAnimated:NO
                       fromSource:gemini::FloatyUpdateSource::Snackbar];
  [_overlay_window activateOverlay:_snackbarView withLevel:UIWindowLevelNormal];
  [_snackbarView
      presentAnimated:YES
           completion:^{
               // The view will now schedule its own dismissal and call
               // the delegate when it's time.
           }];
}

@end

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_suggestion_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/passwords/password_suggestion_view_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
constexpr CGFloat preferredCornerRadius = 20;
}  // namespace

@interface PasswordSuggestionCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordSuggestionViewController* viewController;

// The suggested strong password.
@property(nonatomic, copy) NSString* passwordSuggestion;

// The suggest password decision handler.
@property(nonatomic, copy) void (^decisionHandler)(BOOL accept);

@end

@implementation PasswordSuggestionCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                        passwordSuggestion:(NSString*)passwordSuggestion
                           decisionHandler:
                               (void (^)(BOOL accept))decisionHandler {
  self = [super initWithBaseViewController:baseViewController browser:browser];

  if (self) {
    _passwordSuggestion = passwordSuggestion;
    _decisionHandler = decisionHandler;
  }

  return self;
}

- (void)start {
  self.viewController = [[PasswordSuggestionViewController alloc]
      initWithPasswordSuggestion:self.passwordSuggestion
                       userEmail:[self userEmail]];
  self.viewController.presentationController.delegate = self;
  self.viewController.actionHandler = self;

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(applicationDidEnterBackground:)
                        name:UIApplicationDidEnterBackgroundNotification
                      object:nil];

  if (@available(iOS 15, *)) {
    self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentationController =
        self.viewController.sheetPresentationController;
    presentationController.prefersEdgeAttachedInCompactHeight = YES;
    presentationController.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent
    ];
    presentationController.preferredCornerRadius = preferredCornerRadius;
  } else {
    self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  }

  // Immediately dismiss the keyboard (only on tablet) because the
  // PasswordSuggestion view controller is incorrectly being displayed behind
  // the keyboard. This issue does not happen on mobile devices.
  // For more information, please see: https://www.crbug.com/1307759.
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [self closeKeyboard];
  }

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertSecondaryAction {
  [self handleDecision:NO];
  [self.delegate closePasswordSuggestion];
}

- (void)confirmationAlertPrimaryAction {
  [self handleDecision:YES];
  [self.delegate closePasswordSuggestion];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self handleDecision:NO];
  [self.delegate closePasswordSuggestion];
}

#pragma mark - Notification callback

- (void)applicationDidEnterBackground:(NSNotification*)notification {
  [self confirmationAlertSecondaryAction];
}

#pragma mark - Private methods

// Returns the user email.
- (NSString*)userEmail {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  id<SystemIdentity> authenticatedIdentity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  return authenticatedIdentity.userEmail;
}

- (void)handleDecision:(BOOL)accept {
  if (self.decisionHandler) {
    self.decisionHandler(accept);
  }
}

// Closes the keyboard.
- (void)closeKeyboard {
  NSString* activeWebStateIdentifier = self.browser->GetWebStateList()
                                           ->GetActiveWebState()
                                           ->GetStableIdentifier();
  [self onCloseKeyboardWithIdentifier:activeWebStateIdentifier];
}

// Helper method which closes the keyboard.
- (void)onCloseKeyboardWithIdentifier:(NSString*)identifier {
  Browser* browser = self.browser;
  if (!browser)
    return;
  web::WebState* webState = browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return;
  // Note that it may have changed between the moment the
  // block was created and its invocation. So check whether
  // the WebState identifier is the same.
  NSString* webStateIdentifier = webState->GetStableIdentifier();
  if (![webStateIdentifier isEqualToString:identifier])
    return;
  FormInputAccessoryViewHandler* handler =
      [[FormInputAccessoryViewHandler alloc] init];
  handler.webState = webState;
  NSString* mainFrameID =
      base::SysUTF8ToNSString(web::GetMainWebFrameId(webState));
  [handler setLastFocusFormActivityWebFrameID:mainFrameID];
  [handler closeKeyboardWithoutButtonPress];
}

@end

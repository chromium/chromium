// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#include "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ConsentCoordinator () <ConfirmationAlertActionHandler>

// Base view controller from where |viewController| is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) ConsentViewController* viewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

// The extension context for the credential provider.
@property(nonatomic, weak) ASCredentialProviderExtensionContext* context;

// Interface for |reauthenticationModule|, handling mostly the case when no
// hardware for authentication is available.
@property(nonatomic, weak) ReauthenticationHandler* reauthenticationHandler;

// Indicates if the extension should finish after consent is given.
@property(nonatomic) BOOL isInitialConfigurationRequest;

@end

@implementation ConsentCoordinator

- (instancetype)
       initWithBaseViewController:(UIViewController*)baseViewController
                          context:(ASCredentialProviderExtensionContext*)context
          reauthenticationHandler:
              (ReauthenticationHandler*)reauthenticationHandler
    isInitialConfigurationRequest:(BOOL)isInitialConfigurationRequest {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _context = context;
    _reauthenticationHandler = reauthenticationHandler;
    _isInitialConfigurationRequest = isInitialConfigurationRequest;
  }
  return self;
}

- (void)start {
  self.viewController = [[ConsentViewController alloc] init];
  self.viewController.actionHandler = self;
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
    self.viewController.modalPresentationStyle =
        self.isInitialConfigurationRequest ? UIModalPresentationFullScreen
                                           : UIModalPresentationAutomatic;
  }
  BOOL animated = !self.isInitialConfigurationRequest;
  [self.baseViewController presentViewController:self.viewController
                                        animated:animated
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  NSError* error =
      [[NSError alloc] initWithDomain:ASExtensionErrorDomain
                                 code:ASExtensionErrorCodeUserCanceled
                             userInfo:nil];
  [self.context cancelRequestWithError:error];
}

- (void)confirmationAlertPrimaryAction {
  [self.reauthenticationHandler
      verifyUserWithCompletionHandler:^(ReauthenticationResult result) {
        if (result != ReauthenticationResult::kFailure) {
          NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
          [user_defaults
              setBool:YES
               forKey:kUserDefaultsCredentialProviderConsentVerified];
          if (self.isInitialConfigurationRequest) {
            [self.context completeExtensionConfigurationRequest];
          } else {
            [self.delegate consentCoordinatorDidAcceptConsent:self];
          }
        }
      }
      presentReminderOnViewController:self.viewController];
}

- (void)confirmationAlertSecondaryAction {
  // No-op.
}

- (void)confirmationAlertLearnMoreAction {
  NSString* message =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_MORE_INFO_STRING",
                        @"The information provided in the consent popover.");
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.viewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

@end

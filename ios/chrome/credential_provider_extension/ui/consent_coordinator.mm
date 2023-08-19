// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/consent_coordinator.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/credential_provider_extension/reauthentication_handler.h"
#import "ios/chrome/credential_provider_extension/ui/consent_view_controller.h"
#import "ios/chrome/credential_provider_extension/ui/credential_response_handler.h"

@interface ConsentCoordinator () <PromoStyleViewControllerDelegate>

// Base view controller from where `viewController` is presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The view controller of this coordinator.
@property(nonatomic, strong) ConsentViewController* viewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

// The response handler for the credential configuration.
@property(nonatomic, weak) id<CredentialResponseHandler>
    credentialResponseHandler;

@end

@implementation ConsentCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                 credentialResponseHandler:
                     (id<CredentialResponseHandler>)credentialResponseHandler {
  self = [super init];
  if (self) {
    _baseViewController = baseViewController;
    _credentialResponseHandler = credentialResponseHandler;
  }
  return self;
}

- (void)start {
  self.viewController = [[ConsentViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.modalInPresentation = YES;
  self.viewController.modalPresentationStyle = UIModalPresentationFullScreen;

  [self.baseViewController presentViewController:self.viewController
                                        animated:NO
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

// Invoked when the primary action button is tapped.
- (void)didTapPrimaryActionButton {
  [self.credentialResponseHandler completeExtensionConfigurationRequest];
}

// Invoked when the learn more button is tapped.
- (void)didTapLearnMoreButton {
  NSString* message =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_CONSENT_MORE_INFO_STRING",
                        @"The information provided in the consent popover.");
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
  self.learnMoreViewController.popoverPresentationController.sourceView =
      self.viewController.learnMoreButton.imageView;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

@end

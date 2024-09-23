// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/sharing/activity_services/activity_service_presentation.h"
#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/sharing/sharing_positioner.h"
#import "ios/chrome/browser/ui/sharing/sharing_scenario.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface QRGeneratorCoordinator () <ConfirmationAlertActionHandler> {
  // URL of a page to generate a QR code for.
  GURL _URL;
}

// To be used to handle behaviors that go outside the scope of this class.
@property(nonatomic, strong) id<QRGenerationCommands> handler;

// View controller used to display the QR code and actions.
@property(nonatomic, strong) QRGeneratorViewController* viewController;

// Coordinator for the activity view brought up when the user wants to share
// the QR code.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Title of a page to generate a QR code for.
@property(nonatomic, copy) NSString* title;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

@end

@implementation QRGeneratorCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                     title:(NSString*)title
                                       URL:(const GURL&)URL
                                   handler:(id<QRGenerationCommands>)handler {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _title = title;
    _URL = URL;
    _handler = handler;
  }
  return self;
}

#pragma mark - Chrome Coordinator

- (void)start {
  self.viewController = [[QRGeneratorViewController alloc]
      initWithTitle:self.title
            pageURL:net::NSURLWithGURL(_URL)];

  [self.viewController setModalPresentationStyle:UIModalPresentationFormSheet];
  [self.viewController setActionHandler:self];

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
  [super start];
}

- (void)stop {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
  self.viewController = nil;
  self.learnMoreViewController = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  [self.handler hideQRCode];
}

- (void)confirmationAlertPrimaryAction {
  base::RecordAction(base::UserMetricsAction("MobileShareQRCode"));

  NSString* imageTitle = l10n_util::GetNSStringF(
      IDS_IOS_QR_CODE_ACTIVITY_TITLE, base::SysNSStringToUTF16(self.title));

  SharingParams* params =
      [[SharingParams alloc] initWithImage:self.viewController.content
                                     title:imageTitle
                                  scenario:SharingScenario::QRCodeImage];
  // Configure the image sharing scenario.
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                          params:params
                      originView:self.viewController.primaryActionButton];
  [self.sharingCoordinator start];
}

- (void)confirmationAlertLearnMoreAction {
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_QR_CODE_LEARN_MORE_MESSAGE);
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];

  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.viewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;

  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
}

@end

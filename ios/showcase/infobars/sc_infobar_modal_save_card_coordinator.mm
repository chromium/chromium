// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/infobars/sc_infobar_modal_save_card_coordinator.h"

#import "ios/chrome/browser/autofill/model/message/save_card_message_with_links.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_save_card_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/showcase/infobars/sc_infobar_constants.h"
#import "ios/showcase/infobars/sc_infobar_container_view_controller.h"
#import "url/gurl.h"

class GURL;

@interface SCInfobarModalSaveCardCoordinator () <InfobarBannerDelegate,
                                                 InfobarSaveCardModalDelegate>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong) ContainerViewController* containerViewController;
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
@property(nonatomic, strong)
    InfobarSaveCardTableViewController* modalViewController;
// Consumer that is configured by this coordinator.
@property(nonatomic, weak) id<InfobarSaveCardModalConsumer> modalConsumer;
@end

@implementation SCInfobarModalSaveCardCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[ContainerViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Save Card Infobar";

  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:YES
                  type:InfobarType::kInfobarTypeConfirm];
  self.bannerViewController.titleText = kInfobarBannerTitleLabel;
  self.bannerViewController.subtitleText = kInfobarBannerSubtitleLabel;
  self.bannerViewController.buttonText = kInfobarBannerButtonLabel;
  self.containerViewController.bannerViewController = self.bannerViewController;

  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

#pragma mark InfobarSaveCardModalDelegate

- (void)dismissInfobarModal:(id)infobarModal {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self dismissInfobarModal:infobarModal];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
}

- (void)saveCardWithCardholderName:(NSString*)cardholderName
                   expirationMonth:(NSString*)month
                    expirationYear:(NSString*)year {
}

- (void)dismissModalAndOpenURL:(const GURL&)linkURL {
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self dismissInfobarBannerForUserInteraction:NO];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  [self.baseViewController dismissViewControllerAnimated:YES completion:nil];
}

- (void)presentInfobarModalFromBanner {
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self.containerViewController;

  self.modalViewController =
      [[InfobarSaveCardTableViewController alloc] initWithModalDelegate:self];
  self.modalConsumer = self.modalViewController;
  self.modalViewController.title = @"Title";

  SaveCardMessageWithLinks* message = [[SaveCardMessageWithLinks alloc] init];
  message.messageText = @"Terms of Service";
  std::vector<GURL> linkURLs;
  linkURLs.push_back(GURL("http://www.google.com"));
  message.linkURLs = linkURLs;
  message.linkRanges = [[NSArray alloc]
      initWithObjects:[NSValue valueWithRange:NSMakeRange(0, 5)], nil];

  NSDictionary* prefs = @{
    kCardholderNamePrefKey : @"Visa",
    kCardIssuerIconNamePrefKey :
        DefaultSymbolTemplateWithPointSize(kCreditCardSymbol, 18),
    kCardNumberPrefKey : @"•••• 1234",
    kExpirationMonthPrefKey : @"09",
    kExpirationYearPrefKey : @"2023",
    kLegalMessagesPrefKey : [NSMutableArray arrayWithObject:message],
    kCurrentCardSavedPrefKey : @NO,
    kSupportsEditingPrefKey : @YES
  };
  [self.modalConsumer setupModalViewControllerWithPrefs:prefs];

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = self.modalTransitionDriver;
  navController.modalPresentationStyle = UIModalPresentationCustom;

  [self.bannerViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

- (void)infobarBannerWasDismissed {
  self.bannerViewController = nil;
}

@end

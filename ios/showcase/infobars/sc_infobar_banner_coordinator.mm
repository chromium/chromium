// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/infobars/sc_infobar_banner_coordinator.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"
#import "ios/showcase/infobars/sc_infobar_constants.h"
#import "ios/showcase/infobars/sc_infobar_container_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCInfobarBannerCoordinator () <InfobarBannerDelegate,
                                          InfobarModalDelegate>
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
@property(nonatomic, strong) ContainerViewController* containerViewController;
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;
@property(nonatomic, strong) InfobarModalViewController* modalViewController;
@end

@implementation SCInfobarBannerCoordinator
@synthesize baseViewController = _baseViewController;

- (void)start {
  self.containerViewController = [[ContainerViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Infobar Messages";

  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:self
         presentsModal:YES
                  type:InfobarType::kInfobarTypeConfirm];
  self.bannerViewController.titleText = kInfobarBannerTitleLabel;
  self.bannerViewController.subTitleText = kInfobarBannerSubtitleLabel;
  self.bannerViewController.buttonText = kInfobarBannerButtonLabel;
  self.containerViewController.bannerViewController = self.bannerViewController;

  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

- (void)dealloc {
  [self dismissInfobarBanner:nil animated:YES completion:nil userInitiated:NO];
}

#pragma mark InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self dismissInfobarBanner:nil animated:YES completion:nil userInitiated:NO];
}

- (void)presentInfobarModalFromBanner {
  self.modalTransitionDriver = [[InfobarModalTransitionDriver alloc]
      initWithTransitionMode:InfobarModalTransitionBanner];
  self.modalTransitionDriver.modalPositioner = self.containerViewController;
  self.modalViewController =
      [[InfobarModalViewController alloc] initWithModalDelegate:self];
  self.modalViewController.title = kInfobarBannerPresentedModalLabel;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:self.modalViewController];
  navController.transitioningDelegate = self.modalTransitionDriver;
  navController.modalPresentationStyle = UIModalPresentationCustom;

  [self.bannerViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

- (void)dismissInfobarBanner:(id)sender
                    animated:(BOOL)animated
                  completion:(ProceduralBlock)completion
               userInitiated:(BOOL)userInitiated {
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:nil];
}

- (void)infobarBannerWasDismissed {
  self.bannerViewController = nil;
}

#pragma mark InfobarModalDelegate

- (void)modalInfobarButtonWasAccepted:(id)sender {
  [self dismissInfobarModal:sender animated:YES completion:nil];
}

- (void)dismissInfobarModal:(UIButton*)sender
                   animated:(BOOL)animated
                 completion:(ProceduralBlock)completion {
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:nil];
}

- (void)modalInfobarWasDismissed:(id)sender {
}

@end

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_coordinator.h"

#import "base/check.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_request.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/reading_list/reading_list_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator_util.h"
#import "ios/chrome/browser/ui/util/named_guide.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface InfobarBannerOverlayCoordinator () <InfobarBannerPositioner>
// The list of supported mediator classes.
@property(class, nonatomic, readonly) NSArray<Class>* supportedMediatorClasses;
// The banner view being managed by this coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// The transition delegate used by the coordinator to present the banner.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
@end

@implementation InfobarBannerOverlayCoordinator

#pragma mark - Accessors

+ (NSArray<Class>*)supportedMediatorClasses {
  return @[
    [PasswordInfobarBannerOverlayMediator class],
    [ConfirmInfobarBannerOverlayMediator class],
    [TranslateInfobarBannerOverlayMediator class],
    [SaveCardInfobarBannerOverlayMediator class],
    [SaveAddressProfileInfobarBannerOverlayMediator class],
    [AddToReadingListInfobarBannerOverlayMediator class],
    [PermissionsBannerOverlayMediator class],
    [TailoredSecurityInfobarBannerOverlayMediator class],
    [SyncErrorInfobarBannerOverlayMediator class],
  ];
}

+ (const OverlayRequestSupport*)requestSupport {
  static std::unique_ptr<const OverlayRequestSupport> _requestSupport;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    _requestSupport =
        CreateAggregateSupportForMediators(self.supportedMediatorClasses);
  });
  return _requestSupport.get();
}

#pragma mark - InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  NamedGuide* omniboxGuide =
      [NamedGuide guideWithName:kOmniboxGuide
                           view:self.baseViewController.view];
  UIView* owningView = omniboxGuide.owningView;
  CGRect omniboxFrame = [owningView convertRect:omniboxGuide.layoutFrame
                                         toView:owningView.window];
  return CGRectGetMaxY(omniboxFrame);
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request)
    return;
  // Create the mediator and use it aas the delegate for the banner view.
  InfobarOverlayRequestConfig* config =
      self.request->GetConfig<InfobarOverlayRequestConfig>();
  InfobarBannerOverlayMediator* mediator = [self newMediator];
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:mediator
         presentsModal:config->has_badge()
                  type:config->infobar_type()];
  mediator.consumer = self.bannerViewController;
  self.mediator = mediator;
  // Present the banner.
  self.bannerViewController.modalPresentationStyle = UIModalPresentationCustom;
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  self.bannerViewController.interactionDelegate = self.bannerTransitionDriver;
  __weak InfobarBannerOverlayCoordinator* weakSelf = self;
  [self.baseViewController
      presentViewController:self.viewController
                   animated:animated
                 completion:^{
                   InfobarBannerOverlayCoordinator* strongSelf = weakSelf;
                   if (strongSelf) {
                     [strongSelf finishPresentation];
                   }
                 }];
  self.started = YES;

  if (!UIAccessibilityIsVoiceOverRunning()) {
    // Auto-dismiss the banner after timeout if VoiceOver is off (banner should
    // persist until user explicitly swipes it away).
    const base::TimeDelta timeout =
        config->is_high_priority() ? kInfobarBannerLongPresentationDuration
                                   : kInfobarBannerDefaultPresentationDuration;
    [self performSelector:@selector(dismissBannerIfReady)
               withObject:nil
               afterDelay:timeout.InSecondsF()];
  }
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started)
    return;
  // Mark started as NO before calling dismissal callback to prevent dup
  // stopAnimated: executions.
  self.started = NO;
  __weak InfobarBannerOverlayCoordinator* weakSelf = self;
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:^{
                                                [weakSelf finishDismissal];
                                              }];
}

- (UIViewController*)viewController {
  return self.bannerViewController;
}

#pragma mark - Private

// Called when the presentation of the banner UI is completed.
- (void)finishPresentation {
  // Notify the presentation context that the presentation has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishPresentation(self.request);
  }
  UpdateBannerAccessibilityForPresentation(self.baseViewController,
                                           self.viewController.view);
}

// Called when the dismissal of the banner UI is finished.
- (void)finishDismissal {
  InfobarBannerOverlayMediator* mediator =
      base::mac::ObjCCast<InfobarBannerOverlayMediator>(self.mediator);
  [mediator finishDismissal];
  self.bannerViewController = nil;
  self.mediator = nil;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishDismissal(self.request);
  }
  UpdateBannerAccessibilityForDismissal(self.baseViewController);
}

// Creates a mediator instance from the supported mediator class list that
// supports the coordinator's request.
- (InfobarBannerOverlayMediator*)newMediator {
  InfobarBannerOverlayMediator* mediator =
      base::mac::ObjCCast<InfobarBannerOverlayMediator>(GetMediatorForRequest(
          [self class].supportedMediatorClasses, self.request));
  DCHECK(mediator) << "None of the supported mediator classes support request.";
  return mediator;
}

// Indicate to the UI to dismiss itself if it is ready (e.g. the user is not
// currently interaction with it).
- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

@end

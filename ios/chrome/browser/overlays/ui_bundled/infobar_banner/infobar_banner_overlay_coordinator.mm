// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/infobars/ui_bundled/infobar_constants.h"
#import "ios/chrome/browser/infobars/ui_bundled/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/infobars/ui_bundled/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/features.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator_factory.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

@interface InfobarBannerOverlayCoordinator () <InfobarBannerPositioner>
// The banner view being managed by this coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// The transition delegate used by the coordinator to present the banner.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;
@end

@implementation InfobarBannerOverlayCoordinator

#pragma mark - Accessors

+ (const OverlayRequestSupport*)requestSupport {
  return [InfobarBannerOverlayMediator requestSupport];
}

#pragma mark - InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  if (!self.started || !self.browser) {
    return 0;
  }
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  if (IsCurrentLayoutBottomOmnibox(self.browser)) {
    // Use the top toolbar's layout guide when the omnibox is at the bottom.
    UIView* topToolbar =
        [layoutGuideCenter referencedViewUnderName:kPrimaryToolbarGuide];
    CGRect topToolbarFrame = [topToolbar convertRect:topToolbar.bounds
                                              toView:nil];
    CGFloat topToolbarMaxY =
        CGRectGetMaxY(topToolbarFrame) + kInfobarTopPaddingBottomOmnibox;
    return topToolbarMaxY;
  }

  UIView* topOmnibox =
      [layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
  CGRect omniboxFrame = [topOmnibox convertRect:topOmnibox.bounds toView:nil];
  return CGRectGetMaxY(omniboxFrame);
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark - OverlayRequestCoordinator

- (void)startAnimated:(BOOL)animated {
  if (self.started || !self.request) {
    return;
  }
  // Create the mediator and use it as the delegate for the banner view.
  InfobarOverlayRequestConfig* config =
      self.request->GetConfig<InfobarOverlayRequestConfig>();
  InfobarBannerOverlayMediator* mediator =
      [InfobarBannerOverlayMediator mediatorForRequest:self.request];
  self.bannerViewController = [[InfobarBannerViewController alloc]
      initWithDelegate:mediator
         presentsModal:config->has_badge()
                  type:config->infobar_type()];
  mediator.consumer = self.bannerViewController;
  [mediator
      configureDependenciesWithDispatcher:self.browser->GetCommandDispatcher()];
  mediator.engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(self.profile);

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
                   if (strongSelf && strongSelf.started) {
                     [strongSelf finishPresentation];
                   }
                 }];
  self.started = YES;

  if (!UIAccessibilityIsVoiceOverRunning()) {
    // Auto-dismiss the banner after timeout if VoiceOver is off (banner should
    // persist until user explicitly swipes it away).
    [self performSelector:@selector(dismissBannerIfReady)
               withObject:nil
               afterDelay:[self infobarDuration].InSecondsF()];
  }
}

- (void)stopAnimated:(BOOL)animated {
  if (!self.started) {
    return;
  }
  // Mark started as NO before calling dismissal callback to prevent dup
  // stopAnimated: executions.
  self.started = NO;
  // Disconnect the mediator synchronously so it stops referencing command
  // handlers before asynchronous view controller dismissal finishes.
  [self.mediator disconnect];
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
      base::apple::ObjCCast<InfobarBannerOverlayMediator>(self.mediator);
  [mediator finishDismissal];
  self.bannerViewController = nil;
  self.mediator = nil;
  // Notify the presentation context that the dismissal has finished.  This
  // is necessary to synchronize OverlayPresenter scheduling logic with the UI
  // layer.
  if (self.delegate) {
    self.delegate->OverlayUIDidFinishDismissal(self.requestId);
  }
  UpdateBannerAccessibilityForDismissal(self.baseViewController);
}

// Indicate to the UI to dismiss itself if it is ready (e.g. the user is not
// currently interaction with it).
- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

// Determines the duration for which to show the infobar based on its priority
// and its type.
- (base::TimeDelta)infobarDuration {
  std::optional<base::TimeDelta> overrideDuration =
      tests_hook::GetOverrideInfobarDuration();
  if (overrideDuration.has_value()) {
    return overrideDuration.value();
  }
  InfobarOverlayRequestConfig* config =
      self.request->GetConfig<InfobarOverlayRequestConfig>();

  // Experiments with longer infobar duration for passwords, cards and
  // addresses.
  InfobarType type = config->infobar_type();
  if (type == InfobarType::kInfobarTypePasswordSave ||
      type == InfobarType::kInfobarTypePasswordUpdate) {
    if (base::FeatureList::IsEnabled(kPasswordInfobarDisplayLength)) {
      return base::Seconds(kPasswordInfobarDisplayLengthParam.Get());
    }
  } else if (type == InfobarType::kInfobarTypeSaveCard) {
    if (base::FeatureList::IsEnabled(kCreditCardInfobarDisplayLength)) {
      return base::Seconds(kCreditCardInfobarDisplayLengthParam.Get());
    }
  } else if (type == InfobarType::kInfobarTypeSaveAutofillAddressProfile) {
    if (base::FeatureList::IsEnabled(kAddressInfobarDisplayLength)) {
      return base::Seconds(kAddressInfobarDisplayLengthParam.Get());
    }
  }

  return config->is_high_priority() ? kInfobarBannerLongPresentationDuration
                                    : kInfobarBannerDefaultPresentationDuration;
}

@end

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/overlays/model/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_accessibility_util.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/confirm/confirm_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/features.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/parcel_tracking/parcel_tracking_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/passwords/password_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/safe_browsing/enhanced_safe_browsing_infobar_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/save_card/save_card_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/sync_error/sync_error_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/tailored_security/tailored_security_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/translate/translate_infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator_util.h"

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
    [PermissionsBannerOverlayMediator class],
    [TailoredSecurityInfobarBannerOverlayMediator class],
    [SyncErrorInfobarBannerOverlayMediator class],
    [ParcelTrackingBannerOverlayMediator class],
    [EnhancedSafeBrowsingBannerOverlayMediator class],
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
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  UIView* topOmnibox =
      [layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
  CGRect omniboxFrame = [topOmnibox convertRect:topOmnibox.bounds toView:nil];
  CGFloat omniboxMaxY = CGRectGetMaxY(omniboxFrame);

  // Use the top toolbar's layout guide when the omnibox is at the bottom.
  if (topOmnibox.hidden) {
    UIView* topToolbar =
        [layoutGuideCenter referencedViewUnderName:kPrimaryToolbarGuide];
    CGRect topToolbarFrame = [topToolbar convertRect:topToolbar.bounds
                                              toView:nil];
    CGFloat topToolbarMaxY =
        CGRectGetMaxY(topToolbarFrame) + kInfobarTopPaddingBottomOmnibox;
    return topToolbarMaxY;
  }
  return omniboxMaxY;
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
  mediator.engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(
          self.browser->GetProfile());

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
      base::apple::ObjCCast<InfobarBannerOverlayMediator>(self.mediator);
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
  if (DefaultInfobarOverlayRequestConfig::RequestSupport()->IsRequestSupported(
          self.request)) {
    DefaultInfobarOverlayRequestConfig* config =
        self.request->GetConfig<DefaultInfobarOverlayRequestConfig>();
    return [self mediatorForInfobarType:config->infobar_type()];
  }

  InfobarBannerOverlayMediator* mediator =
      base::apple::ObjCCast<InfobarBannerOverlayMediator>(GetMediatorForRequest(
          [self class].supportedMediatorClasses, self.request));
  DCHECK(mediator) << "None of the supported mediator classes support request.";
  return mediator;
}

// Returns the mediator corresponding to the given `infobarType`.
- (InfobarBannerOverlayMediator*)mediatorForInfobarType:
    (InfobarType)infobarType {
  Class mediatorClass = nil;

  switch (infobarType) {
    case InfobarType::kInfobarTypePasswordSave:
    case InfobarType::kInfobarTypePasswordUpdate:
      mediatorClass = [PasswordInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypePermissions:
      mediatorClass = [PermissionsBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeTailoredSecurityService:
      mediatorClass = [TailoredSecurityInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeSaveCard:
      mediatorClass = [SaveCardInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeSyncError:
      mediatorClass = [SyncErrorInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeTranslate:
      mediatorClass = [TranslateInfobarBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeParcelTracking:
      mediatorClass = [ParcelTrackingBannerOverlayMediator class];
      break;
    case InfobarType::kInfobarTypeEnhancedSafeBrowsing:
      mediatorClass = [EnhancedSafeBrowsingBannerOverlayMediator class];
      break;
    default:
      NOTREACHED() << "Received unsupported infobarType.";
  }

  return [[mediatorClass alloc] initWithRequest:self.request];
}

// Indicate to the UI to dismiss itself if it is ready (e.g. the user is not
// currently interaction with it).
- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

// Determines the duration for which to show the infobar based on its priority
// and its type.
- (base::TimeDelta)infobarDuration {
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

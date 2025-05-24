// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_coordinator.h"

#import "base/notreached.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/default_browser/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/default_promo/ui_bundled/default_browser_promo_non_modal_commands.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/infobars/ui_bundled/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/infobars/ui_bundled/coordinators/infobar_coordinator+subclassing.h"
#import "ios/chrome/browser/infobars/ui_bundled/coordinators/infobar_coordinator_implementation.h"
#import "ios/chrome/browser/shared/coordinator/default_browser_promo/non_modal_default_browser_promo_scheduler_scene_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface DefaultBrowserPromoNonModalCoordinator ()
// InfobarBannerViewController owned by this Coordinator.
@property(nonatomic, strong) InfobarBannerViewController* bannerViewController;
// YES if the Infobar has been Accepted.
@property(nonatomic, assign) BOOL infobarAccepted;

@end

@implementation DefaultBrowserPromoNonModalCoordinator {
  // The reason to show the non modal promo.
  NonModalDefaultBrowserPromoReason _promoReason;
}

// Synthesize because readonly property from superclass is changed to readwrite.
@synthesize bannerViewController = _bannerViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               promoReason:(NonModalDefaultBrowserPromoReason)
                                               promoReason {
  self = [super initWithInfoBarDelegate:nil
                           badgeSupport:YES
                                   type:InfobarType::kInfobarTypeConfirm];
  if (self) {
    self.baseViewController = viewController;
    self.browser = browser;
    self.shouldUseDefaultDismissal = NO;
    _promoReason = promoReason;
  }
  return self;
}

- (void)start {
  if (!self.started) {
    self.started = YES;
    self.bannerViewController =
        [[InfobarBannerViewController alloc] initWithDelegate:self
                                                presentsModal:NO
                                                         type:self.infobarType];
    [self.bannerViewController
        setTitleText:[self defaultBrowserNonModalTitleForPromoReason]];
    [self.bannerViewController
        setSubtitleText:[self defaultBrowserNonModalSubtitleForPromoReason]];
    [self.bannerViewController
        setButtonText:l10n_util::GetNSString(
                          IDS_IOS_DEFAULT_NON_MODAL_PRIMARY_BUTTON_TEXT)];
    UIImage* image = ios::provider::GetBrandedImage(
        ios::provider::BrandedImage::kNonModalDefaultBrowserPromo);
    [self.bannerViewController setIconImage:image];
    [self.bannerViewController setUseIconBackgroundTint:NO];
    [self.bannerViewController setPresentsModal:NO];
    [self recordDefaultBrowserPromoShown];
  }
}

// Overrides the superclass's implementation because that requires an
// infobarDelegate, which this class doesn't have.
- (void)bannerInfobarButtonWasPressed:(id)sender {
  [self performInfobarAction];
}

#pragma mark - InfobarCoordinatorImplementation

- (BOOL)configureModalViewController {
  return NO;
}

- (BOOL)isInfobarAccepted {
  return self.infobarAccepted;
}

- (BOOL)infobarBannerActionWillPresentModal {
  return NO;
}

- (void)infobarBannerWasPresented {
  // No-op.
}

- (void)infobarModalPresentedFromBanner:(BOOL)presentedFromBanner {
  // No-op. Should never happen as the non-modal promo should never have a
  // modal.
}

- (void)dismissBannerIfReady {
  [self.bannerViewController dismissWhenInteractionIsFinished];
}

- (BOOL)infobarActionInProgress {
  return NO;
}

- (void)performInfobarAction {
  self.infobarAccepted = YES;
  SceneState* sceneState = self.browser->GetSceneState();
  [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
      logUserPerformedPromoAction];
}

- (void)infobarBannerWillBeDismissed:(BOOL)userInitiated {
  if (userInitiated) {
    SceneState* sceneState = self.browser->GetSceneState();
    [[NonModalDefaultBrowserPromoSchedulerSceneAgent agentFromScene:sceneState]
        logUserDismissedPromo];
  }
}

- (void)infobarWasDismissed {
  self.bannerViewController = nil;

  if (IsNonModalPromoMigrationEnabled()) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(self.profile);
    tracker->Dismissed(GetFeatureForPromoReason(_promoReason));
  }

  id<DefaultBrowserPromoNonModalCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         DefaultBrowserPromoNonModalCommands);
  [handler defaultBrowserNonModalPromoWasDismissed];
}

- (CGFloat)infobarModalHeightForWidth:(CGFloat)width {
  // The non-modal promo should never have a modal.
  NOTREACHED();
}

#pragma mark - private

// Records that a default browser promo has been shown.
- (void)recordDefaultBrowserPromoShown {
  LogToFETDefaultBrowserPromoShown(
      feature_engagement::TrackerFactory::GetForProfile(self.profile));
}

// Returns the default subtitle for the browser's non-modal window based on the
// promo reason.
- (NSString*)defaultBrowserNonModalSubtitleForPromoReason {
  if (!IsTailoredNonModalDBPromoEnabled()) {
    return l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION);
  }

  switch (_promoReason) {
    case NonModalDefaultBrowserPromoReason::PromoReasonOmniboxPaste:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_DESCRIPTION);
    case NonModalDefaultBrowserPromoReason::PromoReasonAppSwitcher:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_1P_APP_DESCRIPTION);
    case NonModalDefaultBrowserPromoReason::PromoReasonShare:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_SHARE_DESCRIPTION);
    default:
      NOTREACHED();
  }
}

// Returns the default title for the browser's non-modal window based on the
// promo reason.
- (NSString*)defaultBrowserNonModalTitleForPromoReason {
  if (!IsTailoredNonModalDBPromoEnabled()) {
    return l10n_util::GetNSString(
        IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE);
  }

  switch (_promoReason) {
    case NonModalDefaultBrowserPromoReason::PromoReasonOmniboxPaste:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_OMNIBOX_NAVIGATION_TITLE);
    case NonModalDefaultBrowserPromoReason::PromoReasonAppSwitcher:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_1P_APP_TITLE);
    case NonModalDefaultBrowserPromoReason::PromoReasonShare:
      return l10n_util::GetNSString(
          IDS_IOS_DEFAULT_BROWSER_NON_MODAL_SHARE_TITLE);
    default:
      NOTREACHED();
  }
}

@end

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/segmentation_platform/public/segmentation_platform_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/commerce/model/shopping_service_factory.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_detail_coordinator.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_mediator.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_item.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/best_features_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/features.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface BestFeaturesScreenCoordinator () <BestFeaturesDelegate>

@end

@implementation BestFeaturesScreenCoordinator {
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _delegate;
  // Best Features Screen mediator.
  BestFeaturesScreenMediator* _mediator;
  // Transparent view used to block user interaction before the Best Features
  // Screen presents.
  UIView* _transparentView;
  // Best Features Screen view controller.
  BestFeaturesViewController* _viewController;
  // The BestFeaturesScreenDetail coordinator.
  BestFeaturesScreenDetailCoordinator* _detailScreenCoordinator;
  // Whether the user has tapped one of the Best Feature items.
  BOOL _itemTapped;
}
@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  first_run::BestFeaturesScreenVariationType variation =
      first_run::GetBestFeaturesScreenVariationType();

  if (variation == first_run::BestFeaturesScreenVariationType::
                       kSignedInUsersOnlyAfterDBPromo) {
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(self.profile);
    if (!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      // Skip the Best Features Screen if the "signed in users only" arm is
      // enabled and the user is not signed in.
      [_delegate screenWillFinishPresenting];
      return;
    }
  }

  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kBestFeaturesExperienceStart);

  segmentation_platform::SegmentationPlatformService* segmentationService =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          self.profile);
  commerce::ShoppingService* shoppingService =
      commerce::ShoppingServiceFactory::GetForProfile(self.profile);
  _mediator = [[BestFeaturesScreenMediator alloc]
      initWithSegmentationService:segmentationService
                  shoppingService:shoppingService];

  // Retrieve the user's segmentation status before presenting the view if the
  // "shopping" arm is enabled. Otherwise, present the view.
  if (variation == first_run::BestFeaturesScreenVariationType::
                       kShoppingUsersWithFallbackAfterDBPromo) {
    // Present a transparent view to block UI interaction until screen presents.
    // TODO(crbug.com/396480750): This is a temporary solution. If the feature
    // becomes a full launch candidate, consider more polished solutions, like a
    // loading screen.
    _transparentView =
        [[UIView alloc] initWithFrame:self.baseViewController.view.bounds];
    _transparentView.backgroundColor = [UIColor clearColor];
    [self.baseViewController.view addSubview:_transparentView];
    __weak __typeof(self) weakSelf = self;
    [_mediator retrieveShoppingUserSegmentWithCompletion:^{
      [weakSelf presentScreen];
    }];
  } else {
    [self presentScreen];
  }
}

- (void)stop {
  _delegate = nil;
  [_mediator disconnect];
  _mediator = nil;
  _transparentView = nil;
  _viewController = nil;
  [_detailScreenCoordinator stop];
  _detailScreenCoordinator = nil;

  [super stop];
}

#pragma mark - PromoStyleViewController

- (void)didTapPrimaryActionButton {
  if (!_itemTapped) {
    base::UmaHistogramEnumeration(
        kActionOnBestFeaturesMainScreenHistogram,
        BestFeaturesMainScreenActionType::kContinueWithoutInteracting);
  }
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kBestFeaturesExperienceCompletionThroughMainScreen);
  [_delegate screenWillFinishPresenting];
}

#pragma mark - BestFeaturesDelegate

- (void)didTapBestFeaturesItem:(BestFeaturesItem*)item {
  _itemTapped = YES;
  _detailScreenCoordinator = [[BestFeaturesScreenDetailCoordinator alloc]
      initWithBaseNavigationViewController:_baseNavigationController
                                   browser:self.browser
                          bestFeaturesItem:item];
  [self logItemSelection:item.type];
  _detailScreenCoordinator.delegate = _delegate;
  [_detailScreenCoordinator start];
}

#pragma mark - Private

// Presents the Best Features Screen.
- (void)presentScreen {
  [_transparentView removeFromSuperview];
  _transparentView = nil;
  _viewController = [[BestFeaturesViewController alloc] init];
  _mediator.consumer = _viewController;
  _viewController.delegate = self;
  _viewController.bestFeaturesDelegate = self;
  _viewController.modalInPresentation = YES;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

// Logs when user selects a Best features item row.
- (void)logItemSelection:(BestFeaturesItemType)itemType {
  using enum BestFeaturesItemType;
  using enum BestFeaturesMainScreenActionType;
  BestFeaturesMainScreenActionType enumValue;
  switch (itemType) {
    case kLensSearch:
      enumValue = kLensItemTapped;
      break;
    case kEnhancedSafeBrowsing:
      enumValue = kEnhancedSafeBrowsingItemTapped;
      break;
    case kLockedIncognitoTabs:
      enumValue = kLockedIncognitoTabsItemTapped;
      break;
    case kSaveAndAutofillPasswords:
      enumValue = kSharePasswordsItemTapped;
      break;
    case kTabGroups:
      enumValue = kTabGroupsTapped;
      break;
    case kPriceTrackingAndInsights:
      enumValue = kPriceTrackingTapped;
      break;
    case kAutofillPasswordsInOtherApps:
      enumValue = kSaveAutofillPasswordsItemTapped;
      break;
    case kSharePasswordsWithFamily:
      enumValue = kSharePasswordsItemTapped;
      break;
  }
  base::UmaHistogramEnumeration(kActionOnBestFeaturesMainScreenHistogram,
                                enumValue);
}

@end

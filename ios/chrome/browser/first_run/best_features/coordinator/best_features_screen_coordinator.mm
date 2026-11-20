// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_detail_coordinator.h"
#import "ios/chrome/browser/first_run/best_features/coordinator/best_features_screen_mediator.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_delegate.h"
#import "ios/chrome/browser/first_run/best_features/ui/best_features_view_controller.h"
#import "ios/chrome/browser/first_run/best_features/ui/metrics_util.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/first_run/public/features.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface BestFeaturesScreenCoordinator () <BestFeaturesDelegate,
                                             FirstRunScreenDelegate>

@end

@implementation BestFeaturesScreenCoordinator {
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _delegate;
  // Best Features Screen mediator.
  BestFeaturesScreenMediator* _mediator;
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
      [_delegate firstRunScreenCoordinatorWantsToBeStopped:self];
      return;
    }
  }

  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kBestFeaturesExperienceStart);

  _mediator = [[BestFeaturesScreenMediator alloc] init];
  [self presentScreen];
}

- (void)stop {
  _delegate = nil;
  [_mediator disconnect];
  _mediator = nil;
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
  [_delegate firstRunScreenCoordinatorWantsToBeStopped:self];
}

#pragma mark - BestFeaturesDelegate

- (void)didTapBestFeaturesItem:(BestFeaturesItem*)item {
  _itemTapped = YES;

  if (first_run::GetBestFeaturesScreenVariationType() ==
      first_run::BestFeaturesScreenVariationType::kBestOfApp) {
    // Retrieve the Best Features item that should be presented first and the
    // items users can swipe between.
    using enum first_run::BestFeaturesScreenVariationType;
    NSArray<BestFeaturesItem*>* bestFeaturesItems =
        [_mediator bestFeatureItems];
    int startIndex = startIndex =
        [bestFeaturesItems indexOfObjectPassingTest:^BOOL(
                               BestFeaturesItem* obj, NSUInteger idx, BOOL*) {
          return obj.type == item.type;
        }];

    _detailScreenCoordinator = [[BestFeaturesScreenDetailCoordinator alloc]
        initWithBaseNavigationViewController:_baseNavigationController
                                     browser:self.browser
                           bestFeaturesItems:bestFeaturesItems
                                  startIndex:startIndex
                                      source:DetailScreenPresentationSource::
                                                 kBestOfAppFRE];
  } else {
    _detailScreenCoordinator = [[BestFeaturesScreenDetailCoordinator alloc]
        initWithBaseNavigationViewController:_baseNavigationController
                                     browser:self.browser
                            bestFeaturesItem:item
                                      source:DetailScreenPresentationSource::
                                                 kBestFeaturesFRE];
  }

  [self logItemSelection:item.type];
  _detailScreenCoordinator.delegate = self;
  [_detailScreenCoordinator start];
}

#pragma mark - FirstRunScreenDelegate

- (void)firstRunScreenCoordinatorWantsToBeStopped:
    (ChromeCoordinator*)coordinator {
  CHECK_EQ(coordinator, _detailScreenCoordinator, base::NotFatalUntil::M155);
  [_detailScreenCoordinator stop];
  _detailScreenCoordinator = nil;
  [_delegate firstRunScreenCoordinatorWantsToBeStopped:self];
}

#pragma mark - Private

// Presents the Best Features Screen.
- (void)presentScreen {
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
  BestFeaturesMainScreenActionType enumValue;
  switch (itemType) {
    case kLensSearch:
      enumValue = BestFeaturesMainScreenActionType::kLensItemTapped;
      break;
    case kEnhancedSafeBrowsing:
      enumValue =
          BestFeaturesMainScreenActionType::kEnhancedSafeBrowsingItemTapped;
      break;
    case kLockedIncognitoTabs:
      enumValue =
          BestFeaturesMainScreenActionType::kLockedIncognitoTabsItemTapped;
      break;
    case kSaveAndAutofillPasswords:
      enumValue = BestFeaturesMainScreenActionType::kSharePasswordsItemTapped;
      break;
    case kTabGroups:
      enumValue = BestFeaturesMainScreenActionType::kTabGroupsTapped;
      break;
    case kAutofillPasswordsInOtherApps:
      enumValue =
          BestFeaturesMainScreenActionType::kSaveAutofillPasswordsItemTapped;
      break;
    case kSharePasswordsWithFamily:
      enumValue = BestFeaturesMainScreenActionType::kSharePasswordsItemTapped;
      break;
    case kIncognitoBrowsing:
      enumValue =
          BestFeaturesMainScreenActionType::kIncognitoBrowsingItemTapped;
      break;
  }
  base::UmaHistogramEnumeration(kActionOnBestFeaturesMainScreenHistogram,
                                enumValue);
}

@end

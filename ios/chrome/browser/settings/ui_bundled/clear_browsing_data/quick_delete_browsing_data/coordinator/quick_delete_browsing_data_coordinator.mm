// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_browsing_data/coordinator/quick_delete_browsing_data_coordinator.h"

#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover_factory.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/model/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_browsing_data/coordinator/quick_delete_browsing_data_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_browsing_data/ui/quick_delete_browsing_data_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_browsing_data/ui/quick_delete_browsing_data_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/public/quick_delete_other_data_commands.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface QuickDeleteBrowsingDataCoordinator () <
    QuickDeleteBrowsingDataViewControllerDelegate,
    QuickDeleteOtherDataCommands,
    SignoutActionSheetCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation QuickDeleteBrowsingDataCoordinator {
  QuickDeleteBrowsingDataViewController* _viewController;
  UINavigationController* _navigationController;
  QuickDeleteMediator* _mediator;
  // The coordinator for the "Quick Delete Other Data" page.
  QuickDeleteOtherDataCoordinator* _otherDataCoordinator;
  SignoutActionSheetCoordinator* _signoutCoordinator;
  browsing_data::TimePeriod _initialTimeRange;
}

#pragma mark - ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 timeRange:
                                     (browsing_data::TimePeriod)timeRange {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _initialTimeRange = timeRange;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.profile;

  CHECK(!profile->IsOffTheRecord());

  BrowsingDataCounterWrapperProducer* producer =
      [[BrowsingDataCounterWrapperProducer alloc] initWithProfile:profile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForProfile(profile);
  DiscoverFeedService* discoverFeedService =
      DiscoverFeedServiceFactory::GetForProfile(profile);
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(profile);
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  _mediator =
      [[QuickDeleteMediator alloc] initWithPrefs:profile->GetPrefs()
              browsingDataCounterWrapperProducer:producer
                                 identityManager:identityManager
                             browsingDataRemover:browsingDataRemover
                             discoverFeedService:discoverFeedService
                              templateURLService:templateURLService
                                       timeRange:_initialTimeRange
                                 uiBlockerTarget:self.browser->GetSceneState()
                        featureEngagementTracker:tracker];

  _viewController = [[QuickDeleteBrowsingDataViewController alloc] init];
  _viewController.delegate = self;
  _viewController.mutator = _mediator;

  _mediator.consumer = _viewController;
  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopOtherDataCoordinator];

  _signoutCoordinator.delegate = nil;
  [_signoutCoordinator stop];
  _signoutCoordinator = nil;

  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  _viewController.delegate = nil;
  _viewController.mutator = nil;
  _viewController = nil;

  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;

  _mediator.consumer = nil;
  _mediator = nil;
}

#pragma mark - QuickDeleteBrowsingDataViewControllerDelegate

- (void)dismissBrowsingDataPage {
  [self.delegate stopBrowsingDataPage];
}

- (void)showOtherDataPage {
  _otherDataCoordinator = [[QuickDeleteOtherDataCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser];
  _otherDataCoordinator.quickDeleteOtherDataHandler = self;
  [_otherDataCoordinator start];
}

- (void)signOutAndShowActionSheet {
  Browser* browser = self.browser;
  if (!browser) {
    // The C++ model has been destroyed, return early.
    return;
  }

  if (_signoutCoordinator) {
    // An action is already in progress, ignore user's request.
    return;
  }

  constexpr signin_metrics::ProfileSignout signout_source_metric =
      signin_metrics::ProfileSignout::
          kUserClickedSignoutFromClearBrowsingDataPage;
  __weak __typeof(self) weakSelf = self;
  _signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:browser
                            rect:_viewController.view.frame
                            view:_viewController.view
        forceSnackbarOverToolbar:NO
                      withSource:signout_source_metric
                      completion:^(BOOL success, SceneState* scene_state) {
                        [weakSelf handleAuthenticationOperationDidFinish];
                      }];
  _signoutCoordinator.showUnavailableFeatureDialogHeader = YES;
  _signoutCoordinator.delegate = self;
  [_signoutCoordinator start];
}

#pragma mark - QuickDeleteOtherDataCommands

- (void)hideQuickDeleteOtherDataPage {
  [self stopOtherDataCoordinator];
  // TODO(crbug.com/476398061) Move voiceover focus to the appropriate row in
  // QuickDeleteBrowsingDataViewController.
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  _viewController.view.userInteractionEnabled = NO;
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  _viewController.view.userInteractionEnabled = YES;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissBrowsingDataPage];
}

#pragma mark - Private

// Stops the signout coordinator.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_signoutCoordinator);
  _signoutCoordinator.delegate = nil;
  [_signoutCoordinator stop];
  _signoutCoordinator = nil;
}

// Stops the QuickDeleteOtherDataCoordinator.
- (void)stopOtherDataCoordinator {
  _otherDataCoordinator.quickDeleteOtherDataHandler = nil;
  [_otherDataCoordinator stop];
  _otherDataCoordinator = nil;
}

@end

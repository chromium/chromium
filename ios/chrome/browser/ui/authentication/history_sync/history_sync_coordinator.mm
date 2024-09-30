// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

@interface HistorySyncCoordinator () <HistorySyncMediatorDelegate,
                                      PromoStyleViewControllerDelegate>
@end

@implementation HistorySyncCoordinator {
  // History mediator.
  HistorySyncMediator* _mediator;
  // History view controller.
  HistorySyncViewController* _viewController;
  // Pref service.
  raw_ptr<PrefService> _prefService;
  // `YES` if coordinator used during the first run.
  BOOL _firstRun;
  // `YES` if the user's email should be shown in the footer text.
  BOOL _showUserEmail;
  // Whether the History Sync screen is a optional step, that can be skipped
  // if declined too often.
  BOOL _isOptional;
  // `YES` if the opt-in aborted metric should be recorded when the
  // coordinator stops.
  BOOL _recordOptInEndAtStop;
  // Delegate for the history sync coordinator.
  __weak id<HistorySyncCoordinatorDelegate> _delegate;
  // Access point associated with the history opt-in screen.
  signin_metrics::AccessPoint _accessPoint;
}

@synthesize baseNavigationController = _baseNavigationController;

+ (HistorySyncSkipReason)
    getHistorySyncOptInSkipReason:(syncer::SyncService*)syncService
            authenticationService:(AuthenticationService*)authenticationService
                      prefService:(PrefService*)prefService
            isHistorySyncOptional:(BOOL)isOptional {
  if (syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kTabs) ||
      syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kHistory)) {
    // Skip History Sync Opt-in if sync is disabled, or if history or
    // tabs sync is disabled by policy.
    return HistorySyncSkipReason::kSyncForbiddenByPolicies;
  }
  if (!authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show history sync opt-in screen if no signed-in user account.
    return HistorySyncSkipReason::kNotSignedIn;
  }
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();
  if (userSettings->GetSelectedTypes().HasAll(
          {syncer::UserSelectableType::kHistory,
           syncer::UserSelectableType::kTabs})) {
    // History opt-in is already set. This value is kept between signout/signin.
    // In this case the UI can be skipped.
    return HistorySyncSkipReason::kAlreadyOptedIn;
  }

  if (history_sync::IsDeclinedTooOften(prefService) && isOptional) {
    return HistorySyncSkipReason::kDeclinedTooOften;
  }

  return HistorySyncSkipReason::kNone;
}

+ (void)recordHistorySyncSkipMetric:(HistorySyncSkipReason)reason
                        accessPoint:(signin_metrics::AccessPoint)accessPoint {
  switch (reason) {
    case HistorySyncSkipReason::kNotSignedIn:
    case HistorySyncSkipReason::kSyncForbiddenByPolicies:
    case HistorySyncSkipReason::kDeclinedTooOften:
      base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Skipped"));
      base::UmaHistogramEnumeration(
          "Signin.HistorySyncOptIn.Skipped", accessPoint,
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      break;
    case HistorySyncSkipReason::kAlreadyOptedIn:
      base::RecordAction(
          base::UserMetricsAction("Signin_HistorySync_AlreadyOptedIn"));
      base::UmaHistogramEnumeration(
          "Signin.HistorySyncOptIn.AlreadyOptedIn", accessPoint,
          signin_metrics::AccessPoint::ACCESS_POINT_MAX);
      break;
    case HistorySyncSkipReason::kNone:
      // This method should not be called if the screen should be shown.
      // If a metric should be recorded in this case, it should be handled in
      // HistorySyncCoordinator instance methods instead of this class method
      // to avoid duplicated recording.
      NOTREACHED();
  }
}

- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                            delegate:
                                (id<HistorySyncCoordinatorDelegate>)delegate
                            firstRun:(BOOL)firstRun
                       showUserEmail:(BOOL)showUserEmail
                          isOptional:(BOOL)isOptional
                         accessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _firstRun = firstRun;
    _showUserEmail = showUserEmail;
    _isOptional = isOptional;
    _delegate = delegate;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)start {
  [super start];
  ProfileIOS* profile = self.browser->GetProfile();
  CHECK_EQ(profile, profile->GetOriginalProfile());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  _prefService = profile->GetPrefs();
  // Check if History Sync Opt-In should be skipped.
  HistorySyncSkipReason skipReason = [HistorySyncCoordinator
      getHistorySyncOptInSkipReason:syncService
              authenticationService:authenticationService
                        prefService:_prefService
              isHistorySyncOptional:_isOptional];
  if (skipReason != HistorySyncSkipReason::kNone) {
    [HistorySyncCoordinator recordHistorySyncSkipMetric:skipReason
                                            accessPoint:_accessPoint];
    [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
    return;
  }

  _viewController = [[HistorySyncViewController alloc] init];
  _viewController.delegate = self;

  ChromeAccountManagerService* chromeAccountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  _mediator = [[HistorySyncMediator alloc]
      initWithAuthenticationService:authenticationService
        chromeAccountManagerService:chromeAccountManagerService
                    identityManager:identityManager
                        syncService:syncService
                      showUserEmail:_showUserEmail];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;

  if (_firstRun) {
    _viewController.modalInPresentation = YES;
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kHistorySyncScreenStart);
  }
  base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Started"));
  base::UmaHistogramEnumeration("Signin.HistorySyncOptIn.Started", _accessPoint,
                                signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  _recordOptInEndAtStop = YES;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  [super stop];
  _delegate = nil;
  if (_recordOptInEndAtStop) {
    // Tracks the case where the opt-in flow ends without being accepted or
    // declined by the user. (eg. identity disappeared, popup swiped down by the
    // user, Chrome shutdown)
    //
    // This can also occur during the FRE, for instance if Chrome shuts down
    // when the screen is shown, or if FRE is dismissed due to policies change.
    base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Aborted"));
    base::UmaHistogramEnumeration(
        "Signin.HistorySyncOptIn.Aborted", _accessPoint,
        signin_metrics::AccessPoint::ACCESS_POINT_MAX);
    _recordOptInEndAtStop = NO;
  }
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator.delegate = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
  _prefService = nullptr;
}

- (void)dealloc {
  DUMP_WILL_BE_CHECK(!_viewController);
}

#pragma mark - HistorySyncMediatorDelegate

- (void)historySyncMediatorPrimaryAccountCleared:
    (HistorySyncMediator*)mediator {
  [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_mediator enableHistorySyncOptin];

  history_sync::ResetDeclinePrefs(_prefService);
  base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Completed"));
  [self recordActionButtonTappedWithHistorySyncCompleted:YES];
  if (_firstRun) {
    base::UmaHistogramEnumeration(
        first_run::kFirstRunStageHistogram,
        first_run::kHistorySyncScreenCompletionWithSync);
  }
  base::UmaHistogramEnumeration("Signin.HistorySyncOptIn.Completed",
                                _accessPoint,
                                signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  _recordOptInEndAtStop = NO;

  [_delegate closeHistorySyncCoordinator:self declinedByUser:NO];
}

- (void)didTapSecondaryActionButton {
  history_sync::RecordDeclinePrefs(_prefService);
  base::RecordAction(base::UserMetricsAction("Signin_HistorySync_Declined"));
  [self recordActionButtonTappedWithHistorySyncCompleted:NO];
  if (_firstRun) {
    base::UmaHistogramEnumeration(
        first_run::kFirstRunStageHistogram,
        first_run::kHistorySyncScreenCompletionWithoutSync);
  }
  base::UmaHistogramEnumeration("Signin.HistorySyncOptIn.Declined",
                                _accessPoint,
                                signin_metrics::AccessPoint::ACCESS_POINT_MAX);
  _recordOptInEndAtStop = NO;

  [_delegate closeHistorySyncCoordinator:self declinedByUser:YES];
}

#pragma mark - Private

- (void)recordActionButtonTappedWithHistorySyncCompleted:(BOOL)completed {
  std::optional<signin_metrics::SyncButtonClicked> buttonClicked;
  switch (_viewController.actionButtonsVisibility) {
    case ActionButtonsVisibility::kDefault:
    case ActionButtonsVisibility::kRegularButtonsShown:
      buttonClicked = completed ? signin_metrics::SyncButtonClicked::
                                      kHistorySyncOptInNotEqualWeighted
                                : signin_metrics::SyncButtonClicked::
                                      kHistorySyncCancelNotEqualWeighted;
      break;
    case ActionButtonsVisibility::kEquallyWeightedButtonShown:
      buttonClicked = completed ? signin_metrics::SyncButtonClicked::
                                      kHistorySyncOptInEqualWeighted
                                : signin_metrics::SyncButtonClicked::
                                      kHistorySyncCancelEqualWeighted;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  base::UmaHistogramEnumeration("Signin.SyncButtons.Clicked", *buttonClicked);
}

@end

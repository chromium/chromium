// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_popup_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface HistorySyncPopupCoordinator () <
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation HistorySyncPopupCoordinator {
  // Authentication service.
  raw_ptr<AuthenticationService> _authenticationService;
  // Coordinator to display the tangible sync view.
  HistorySyncCoordinator* _historySyncCoordinator;
  // Navigation controller created for the popup.
  UINavigationController* _navigationController;
  // Whether the account email should be shown in the footer.
  // Should be `NO` if the user has seen the account info by signing in just
  // before seeing the history opt-in screen.
  BOOL _showUserEmail;
  // `YES` if the user should be signed-out if history sync is declined. It
  // should be done for entry points dedicated to history sync instead of
  // sign-in.
  BOOL _signOutIfDeclined;
  // Whether the History Sync screen is a optional step, that can be skipped
  // if declined too often.
  BOOL _isOptional;
  // Used to customize content on screen.
  SigninContextStyle _contextStyle;
  // Access point associated with the history opt-in screen.
  signin_metrics::AccessPoint _accessPoint;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                             showUserEmail:(BOOL)showUserEmail
                         signOutIfDeclined:(BOOL)signOutIfDeclined
                                isOptional:(BOOL)isOptional
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _showUserEmail = showUserEmail;
    _signOutIfDeclined = signOutIfDeclined;
    _isOptional = isOptional;
    _contextStyle = contextStyle;
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)dealloc {
  // TODO(crbug.com/40272467)
  DUMP_WILL_BE_CHECK(!_historySyncCoordinator);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  ProfileIOS* profile = self.profile;
  CHECK_EQ(profile, profile->GetOriginalProfile());
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  // Check if History Sync Opt-In should be skipped.
  CHECK_EQ(history_sync::GetSkipReason(syncService, _authenticationService,
                                       profile->GetPrefs(), _isOptional),
           history_sync::HistorySyncSkipReason::kNone);

  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;
  _historySyncCoordinator = [[HistorySyncCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                              delegate:self
                              firstRun:NO
                         showUserEmail:_showUserEmail
                            isOptional:_isOptional
                          contextStyle:_contextStyle
                           accessPoint:_accessPoint];
  [_historySyncCoordinator start];
  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [self stopHistorySyncCoordinator];
  _navigationController.presentationController.delegate = nil;
  [_navigationController dismissViewControllerAnimated:animated completion:nil];
  _navigationController = nil;
  [super stopAnimated:animated];
}

#pragma mark - Private

- (void)stopHistorySyncCoordinator {
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
}

- (void)viewWasDismissedWithResult:(HistorySyncResult)result {
  if (result == HistorySyncResult::kUserCanceled && _signOutIfDeclined) {
    signin::ProfileSignoutRequest(
        signin_metrics::ProfileSignout::
            kUserDeclinedHistorySyncAfterDedicatedSignIn)
        .Run(self.browser);
  }
  [self.delegate historySyncPopupCoordinator:self didFinishWithResult:result];
}

#pragma mark - HistorySyncCoordinatorDelegate

- (void)historySyncCoordinator:(HistorySyncCoordinator*)historySyncCoordinator
                    withResult:(HistorySyncResult)result {
  [self stopHistorySyncCoordinator];
  __weak __typeof(self) weakSelf = self;
  [_navigationController
      dismissViewControllerAnimated:YES
                         completion:^() {
                           [weakSelf viewWasDismissedWithResult:result];
                         }];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // This should be triggered only if user dismisses the screen manually.
  base::RecordAction(base::UserMetricsAction("Signin_HistorySync_SwipedDown"));
  [self stopHistorySyncCoordinator];
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  [self viewWasDismissedWithResult:HistorySyncResult::kUserCanceled];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"<%@: %p, authenticationService: %p, historySyncCoordinator: %@, "
          @"presented: %@, accessPoint: %d>",
          self.class.description, self, _authenticationService.get(),
          _historySyncCoordinator,
          ViewControllerPresentationStatusDescription(_navigationController),
          static_cast<int>(_accessPoint)];
}

@end

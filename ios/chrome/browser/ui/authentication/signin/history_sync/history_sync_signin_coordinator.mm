// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/history_sync/history_sync_signin_coordinator.h"

#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"

@interface HistorySyncSigninCoordinator () <HistorySyncPopupCoordinatorDelegate>
@end

@implementation HistorySyncSigninCoordinator {
  HistorySyncPopupCoordinator* _syncPopupCoordinator;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();
  BOOL alreadyOptIn = userSettings->GetSelectedTypes().HasAll(
      {syncer::UserSelectableType::kHistory,
       syncer::UserSelectableType::kTabs});
  CHECK(!alreadyOptIn);

  [super start];
  _syncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   showUserEmail:NO
               signOutIfDeclined:NO
                      isOptional:NO
                     accessPoint:self.accessPoint];
  _syncPopupCoordinator.delegate = self;
  [_syncPopupCoordinator start];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  [_syncPopupCoordinator interruptWithAction:action completion:completion];
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result {
  id<SystemIdentity> identity;
  if (result == SigninCoordinatorResultSuccess) {
    AuthenticationService* authService =
        AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
    identity = authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  }

  [self runCompletionWithSigninResult:result completionIdentity:identity];
}

@end

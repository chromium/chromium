// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_coordinator.h"

#import "base/check.h"
#import "base/memory/scoped_refptr.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/remote_actor_credential_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/remote_actor_credential_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/passwords/model/remote_actor/ios_chrome_remote_actor_credential_sharing_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/web/public/web_state.h"

@interface RemoteActorCredentialBottomSheetCoordinator () <
    RemoteActorCredentialBottomSheetMediatorDelegate>
@end

@implementation RemoteActorCredentialBottomSheetCoordinator {
  // Domain for which credentials were requested.
  NSString* _domain;

  // GAIA ID of the requesting user.
  NSString* _gaiaID;

  // Task ID associated with the remote actor request.
  NSString* _taskID;

  // Completion block called with the result of the flow.
  RemoteActorCredentialBottomSheetCompletion _completion;

  // Mediator for the remote actor credential bottom sheet.
  RemoteActorCredentialBottomSheetMediator* _mediator;

  // View controller for the remote actor credential bottom sheet.
  RemoteActorCredentialBottomSheetViewController* _viewController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    domain:(NSString*)domain
                                    gaiaID:(NSString*)gaiaID
                                    taskID:(NSString*)taskID
                                completion:
                                    (RemoteActorCredentialBottomSheetCompletion)
                                        completion {
  CHECK(completion);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _domain = [domain copy];
    _gaiaID = [gaiaID copy];
    _taskID = [taskID copy];
    _completion = [completion copy];
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile;
  CHECK(profile);

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  PasswordTabHelper* passwordTabHelper =
      activeWebState ? PasswordTabHelper::FromWebState(activeWebState)
                     : nullptr;
  password_manager::PasswordManagerClient* passwordManagerClient =
      passwordTabHelper ? passwordTabHelper->GetPasswordManagerClient()
                        : nullptr;

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  CHECK(faviconLoader);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  scoped_refptr<password_manager::PasswordStoreInterface> profilePasswordStore =
      IOSChromeProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  scoped_refptr<password_manager::PasswordStoreInterface> accountPasswordStore =
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  password_manager::RemoteActorCredentialSharingService* sharingService =
      IOSChromeRemoteActorCredentialSharingServiceFactory::GetForProfile(
          profile);

  _mediator = [[RemoteActorCredentialBottomSheetMediator alloc]
      initWithIdentityManager:identityManager
                  syncService:syncService
         profilePasswordStore:profilePasswordStore.get()
         accountPasswordStore:accountPasswordStore.get()
               sharingService:sharingService
        passwordManagerClient:passwordManagerClient
                faviconLoader:faviconLoader
                       domain:_domain
                       gaiaID:_gaiaID
                       taskID:_taskID];
  _mediator.delegate = self;
  [_mediator start];

  _viewController =
      [[RemoteActorCredentialBottomSheetViewController alloc] init];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;

  [_mediator disconnect];
  _mediator = nil;

  // Ensure the caller is always notified, even if the flow is torn down
  // before it finished.
  [self runCompletionWithSuccess:NO];

  [super stop];
}

#pragma mark - RemoteActorCredentialBottomSheetMediatorDelegate

- (void)mediatorDidRequestPresentation:
    (RemoteActorCredentialBottomSheetMediator*)mediator {
  // TODO(crbug.com/579010739): Present the bottom sheet view controller.
}

- (void)mediator:(RemoteActorCredentialBottomSheetMediator*)mediator
    didFinishWithSuccess:(BOOL)success {
  // The owner is responsible for stopping the coordinator once notified.
  [self runCompletionWithSuccess:success];
}

#pragma mark - Private

// Runs `_completion` with `success` if it hasn't already been run.
- (void)runCompletionWithSuccess:(BOOL)success {
  if (!_completion) {
    return;
  }
  RemoteActorCredentialBottomSheetCompletion completion = _completion;
  _completion = nil;
  completion(success);
}

@end

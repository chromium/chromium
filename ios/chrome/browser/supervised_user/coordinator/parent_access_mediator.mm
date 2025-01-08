// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"

#import <memory>

#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_constants.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_view_controller_delegate.h"

@implementation ParentAccessMediator {
  // TODO(crbug.com/384514294): Handle changes in the identity state while
  // permissions are in progress and bottom sheet is displayed.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<SystemIdentityManager> _systemIdentityManager;
}

- (instancetype)initWithAccountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                              identityManager:
                                  (signin::IdentityManager*)identityManager
                        systemIdentityManager:
                            (SystemIdentityManager*)systemIdentityManager {
  if ((self = [super init])) {
    CHECK(accountManagerService);
    CHECK(identityManager);
    CHECK(systemIdentityManager);
    _accountManagerService = accountManagerService;
    _identityManager = identityManager;
    _systemIdentityManager = systemIdentityManager;
  }
  return self;
}

#pragma mark - ParentAccessViewControllerDelegate

- (void)handleParentAccessRequest:(AuthenticatedURLCallback)callback {
  id<SystemIdentity> identity = signin::GetDefaultIdentityOnDevice(
      _identityManager, _accountManagerService);
  _systemIdentityManager->FetchTokenAuthURL(identity, ParentAccessURL(),
                                            std::move(callback));
}

@end

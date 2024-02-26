// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AccountsMediator () <AccountsModelIdentityDataSource,
                                ChromeAccountManagerServiceObserver,
                                IdentityManagerObserverBridgeDelegate,
                                SyncObserverModelBridge>
@end

@implementation AccountsMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Chrome account manager service observer bridge.
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  raw_ptr<syncer::SyncService> _syncService;
  std::unique_ptr<SyncObserverBridge> _syncObserver;

  // The type of account error that is being displayed in the error section for
  // syncing accounts. Is set to kNone when there is no error section.
  syncer::SyncService::UserActionableError _diplayedAccountErrorType;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService
                        authService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _authService = authService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _syncService = syncService;
    _syncObserver = std::make_unique<SyncObserverBridge>(self, _syncService);
    _diplayedAccountErrorType = syncer::SyncService::UserActionableError::kNone;
  }
  return self;
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _accountManagerServiceObserver.reset();
  _authService = nullptr;
  _identityManagerObserver.reset();
  _identityManager = nullptr;
  _syncObserver.reset();
  _syncService = nullptr;
}

#pragma mark - AccountsModelIdentityDataSource

- (id<SystemIdentity>)identityForAccount:(CoreAccountInfo)account {
  return _accountManagerService->GetIdentityWithGaiaID(account.gaia);
}

- (UIImage*)identityAvatarWithSizeForIdentity:(id<SystemIdentity>)identity
                                         size:(IdentityAvatarSize)size {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::TableViewIcon);
}

- (BOOL)isAccountSignedInNotSyncing {
  // TODO(crbug.com/40066949): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  return base::FeatureList::IsEnabled(
             syncer::kReplaceSyncPromosWithSignInPromos) &&
         !_syncService->HasSyncConsent();
}

- (AccountErrorUIInfo*)accountErrorUIInfo {
  return GetAccountErrorUIInfo(_syncService);
}

- (std::vector<CoreAccountInfo>)accountsWithRefreshTokens {
  return _identityManager->GetAccountsWithRefreshTokens();
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self.consumer updateAccountIdentity:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40067367): This method can be removed once
  // crbug.com/40067367 is fixed.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onEndBatchOfRefreshTokenStateChanges {
  if (!_authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This accounts table view will be popped or dismissed when the user
    // is signed out. Avoid reloading it in that case as that would lead to an
    // empty table view.
    return;
  }

  [self.consumer reloadAllItems];
  // Only attempt to pop the top-most view controller once the account list
  // has been dismissed.
  [self.consumer popView];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  [self.consumer updateErrorSectionModelAndReloadViewIfNeeded:YES];
}

@end

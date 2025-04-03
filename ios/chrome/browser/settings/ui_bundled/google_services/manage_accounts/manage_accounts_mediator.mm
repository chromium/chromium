// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/identity_view_item.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface ManageAccountsMediator () <IdentityManagerObserverBridgeDelegate>
@end

@implementation ManageAccountsMediator {
  // Account manager service to retrieve Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  raw_ptr<AuthenticationService> _authService;
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
}

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _accountManagerService = accountManagerService;
    _authService = authService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }
  return self;
}

- (void)disconnect {
  _accountManagerService = nullptr;
  _authService = nullptr;
  _identityManager = nullptr;
  _identityManagerObserver.reset();
}

#pragma mark - AccountsModelIdentityDataSource

- (id<SystemIdentity>)identityWithGaiaID:(NSString*)gaiaID {
  return _accountManagerService->GetIdentityOnDeviceWithGaiaID(gaiaID);
}

- (UIImage*)identityAvatarWithSizeForIdentity:(id<SystemIdentity>)identity
                                         size:(IdentityAvatarSize)size {
  return _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::TableViewIcon);
}

- (IdentityViewItem*)primaryIdentityViewItem {
  return [self identityViewItemForIdentity:_authService->GetPrimaryIdentity(
                                               signin::ConsentLevel::kSignin)];
}

- (std::vector<IdentityViewItem*>)identityViewItems {
  std::vector<IdentityViewItem*> identityViewItemsForAccounts;

  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      signin::GetIdentitiesOnDevice(_identityManager, _accountManagerService);

  for (id<SystemIdentity> identity in identitiesOnDevice) {
    identityViewItemsForAccounts.push_back(
        [self identityViewItemForIdentity:identity]);
  }
  return identityViewItemsForAccounts;
}

#pragma mark - ManageAccountsMutator

- (void)requestRemoveIdentityWithGaiaID:(NSString*)gaiaID
                               itemView:(UIView*)itemView {
  [self.delegate handleRemoveIdentity:[self identityWithGaiaID:gaiaID]
                             itemView:itemView];
}

- (void)requestAddIdentityToDevice {
  [self.delegate showAddAccountToDevice];
}

- (void)requestSignOutWithItemView:(UIView*)itemView {
  [self.delegate signOutWithItemView:itemView];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  id<SystemIdentity> identity =
      _accountManagerService->GetIdentityOnDeviceWithGaiaID(info.gaia);
  [self handleIdentityUpdated:identity];
}

- (void)onAccountsOnDeviceChanged {
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

#pragma mark - Private

- (void)handleIdentityUpdated:(id<SystemIdentity>)identity {
  [self.consumer
      updateIdentityViewItem:[self identityViewItemForIdentity:identity]];
}

- (IdentityViewItem*)identityViewItemForIdentity:(id<SystemIdentity>)identity {
  IdentityViewItem* identityViewItem = [[IdentityViewItem alloc] init];
  identityViewItem.userEmail = identity.userEmail;
  identityViewItem.userFullName = identity.userFullName;
  identityViewItem.gaiaID = identity.gaiaID;
  identityViewItem.managed = [self isIdentityKnownToBeManaged:identity];
  IdentityAvatarSize avatarSize = IsIdentityDiscAccountMenuEnabled()
                                      ? IdentityAvatarSize::Regular
                                      : IdentityAvatarSize::TableViewIcon;
  identityViewItem.avatar = [self identityAvatarWithSizeForIdentity:identity
                                                               size:avatarSize];
  identityViewItem.accessibilityIdentifier = identity.userEmail;
  return identityViewItem;
}

// Returns true if `identity` is known to be managed.
// Returns false if the identity is known not to be managed or if the management
// status is unknown. If the management status is unknown, it is fetched by
// calling `FetchManagedStatusForIdentity`. `handleIdentityUpdated` will be
// called asynchronously when the management status if retrieved and the
// identity is managed.
- (BOOL)isIdentityKnownToBeManaged:(id<SystemIdentity>)identity {
  if (std::optional<BOOL> managed = IsIdentityManaged(identity);
      managed.has_value()) {
    return managed.value();
  }

  __weak __typeof(self) weakSelf = self;
  FetchManagedStatusForIdentity(identity, base::BindOnce(^(bool managed) {
                                  if (managed) {
                                    [weakSelf handleIdentityUpdated:identity];
                                  }
                                }));
  return NO;
}
@end

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"
#import "ios/web_view/public/cwv_sync_errors.h"

namespace {

using AccessTokenInfo = DeviceAccountsProvider::AccessTokenInfo;
using AccessTokenResult = DeviceAccountsProvider::AccessTokenResult;

// Helper function converting `error` for `identity` to an
// AuthenticationErrorCategory.
AuthenticationErrorCategory AuthenticationErrorCategoryFromError(
    CWVIdentity* identity,
    NSError* error) {
  DCHECK(error);

  CWVSyncError sync_error =
      [CWVSyncController.dataSource syncErrorForNSError:error
                                               identity:identity];
  switch (sync_error) {
    case CWVSyncErrorInvalidGAIACredentials:
      return kAuthenticationErrorCategoryAuthorizationErrors;
    case CWVSyncErrorUserNotSignedUp:
      return kAuthenticationErrorCategoryUnknownIdentityErrors;
    case CWVSyncErrorConnectionFailed:
      return kAuthenticationErrorCategoryNetworkServerErrors;
    case CWVSyncErrorServiceUnavailable:
      return kAuthenticationErrorCategoryAuthorizationForbiddenErrors;
    case CWVSyncErrorRequestCanceled:
      return kAuthenticationErrorCategoryUserCancellationErrors;
    case CWVSyncErrorUnexpectedServiceResponse:
      return kAuthenticationErrorCategoryUnknownErrors;
  }
}

// Helper function converting the result of fetching the access token from
// what CWVSyncControllerDataSource pass to the callback to what is expected
// for AccessTokenCallback.
AccessTokenResult AccessTokenResultFrom(NSString* token,
                                        NSDate* expiration,
                                        CWVIdentity* identity,
                                        NSError* error) {
  if (error) {
    return base::unexpected(
        AuthenticationErrorCategoryFromError(identity, error));
  }

  AccessTokenInfo info{base::SysNSStringToUTF8(token),
                       base::Time::FromNSDate(expiration)};

  return base::ok(std::move(info));
}

}  // namespace

WebViewDeviceAccountsProviderImpl::WebViewDeviceAccountsProviderImpl() {}

WebViewDeviceAccountsProviderImpl::~WebViewDeviceAccountsProviderImpl() =
    default;

void WebViewDeviceAccountsProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  DCHECK(CWVSyncController.dataSource);

  NSMutableArray<NSString*>* scopes_array = [NSMutableArray array];
  for (const auto& scope : scopes) {
    [scopes_array addObject:base::SysUTF8ToNSString(scope)];
  }

  // AccessTokenCallback is non-copyable. Using __block allocates the memory
  // directly in the block object at compilation time (instead of doing a
  // copy). This is required to have correct interaction between move-only
  // types and Objective-C blocks.
  __block AccessTokenCallback scoped_callback = std::move(callback);
  CWVIdentity* identity =
      [[CWVIdentity alloc] initWithEmail:nil
                                fullName:nil
                                  gaiaID:base::SysUTF8ToNSString(gaia_id)];
  [CWVSyncController.dataSource
      fetchAccessTokenForIdentity:identity
                           scopes:scopes_array
                completionHandler:^(NSString* access_token,
                                    NSDate* expiration_date, NSError* error) {
                  std::move(scoped_callback)
                      .Run(AccessTokenResultFrom(access_token, expiration_date,
                                                 identity, error));
                }];
}

std::vector<DeviceAccountsProvider::AccountInfo>
WebViewDeviceAccountsProviderImpl::GetAllAccounts() const {
  DCHECK(CWVSyncController.dataSource);

  NSArray<CWVIdentity*>* identities =
      [CWVSyncController.dataSource allKnownIdentities];
  std::vector<AccountInfo> account_infos;
  for (CWVIdentity* identity in identities) {
    AccountInfo account_info;
    account_info.email = base::SysNSStringToUTF8(identity.email);
    account_info.gaia = base::SysNSStringToUTF8(identity.gaiaID);
    account_infos.push_back(account_info);
  }
  return account_infos;
}

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
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

void WebViewDeviceAccountsProviderImpl::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void WebViewDeviceAccountsProviderImpl::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void WebViewDeviceAccountsProviderImpl::GetAccessToken(
    const GaiaId& gaia_id,
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
                                  gaiaID:gaia_id.ToNSString()];
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
WebViewDeviceAccountsProviderImpl::GetAccountsForProfile() const {
  // WebView doesn't have profiles, so the accounts for this profile are the
  // same as the accounts on the device.
  return GetAccountsOnDevice();
}

std::vector<DeviceAccountsProvider::AccountInfo>
WebViewDeviceAccountsProviderImpl::GetAccountsOnDevice() const {
  DCHECK(CWVSyncController.dataSource);

  NSArray<CWVIdentity*>* identities =
      [CWVSyncController.dataSource allKnownIdentities];
  std::vector<AccountInfo> account_infos;
  for (CWVIdentity* identity in identities) {
    account_infos.emplace_back(GaiaId(identity.gaiaID),
                               base::SysNSStringToUTF8(identity.email),
                               /*hosted_domain=*/"");
  }
  return account_infos;
}

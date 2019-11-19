// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/signin/device_accounts_provider_impl.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/signin_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the account info for |identity| (which must not be nil).
DeviceAccountsProvider::AccountInfo GetAccountInfo(
    ChromeIdentity* identity,
    ios::ChromeIdentityService* identity_service) {
  DCHECK(identity);
  DeviceAccountsProvider::AccountInfo account_info;
  account_info.gaia = base::SysNSStringToUTF8([identity gaiaID]);
  account_info.email = base::SysNSStringToUTF8([identity userEmail]);

  // If hosted domain is nil, then it means the information has not been
  // fetched from gaia; in that case, set account_info.hosted_domain to
  // an empty string. Otherwise, set it to the value of the hostedDomain
  // or kNoHostedDomainFound if the string is empty.
  NSString* hostedDomain =
      identity_service->GetCachedHostedDomainForIdentity(identity);
  if (hostedDomain) {
    account_info.hosted_domain = [hostedDomain length]
                                     ? base::SysNSStringToUTF8(hostedDomain)
                                     : kNoHostedDomainFound;
  }
  return account_info;
}
}

DeviceAccountsProviderImpl::DeviceAccountsProviderImpl() {}

DeviceAccountsProviderImpl::~DeviceAccountsProviderImpl() {}

void DeviceAccountsProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  DCHECK(!callback.is_null());
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();

  // AccessTokenCallback is non-copyable. Using __block allocates the memory
  // directly in the block object at compilation time (instead of doing a
  // copy). This is required to have correct interaction between move-only
  // types and Objective-C blocks.
  __block AccessTokenCallback scopedCallback = std::move(callback);
  identity_service->GetAccessToken(
      identity_service->GetIdentityWithGaiaID(gaia_id), client_id, scopes,
      ^(NSString* token, NSDate* expiration, NSError* error) {
        std::move(scopedCallback).Run(token, expiration, error);
      });
}

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProviderImpl::GetAllAccounts() const {
  std::vector<AccountInfo> accounts;
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  NSArray* identities = identity_service->GetAllIdentities();
  for (ChromeIdentity* identity in identities) {
    accounts.push_back(GetAccountInfo(identity, identity_service));
  }
  return accounts;
}

AuthenticationErrorCategory
DeviceAccountsProviderImpl::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  DCHECK(error);
  if ([error.domain isEqualToString:kAuthenticationErrorDomain] &&
      error.code == NO_AUTHENTICATED_USER) {
    return kAuthenticationErrorCategoryUnknownIdentityErrors;
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  if (identity_service->IsMDMError(
          identity_service->GetIdentityWithGaiaID(gaia_id), error)) {
    return kAuthenticationErrorCategoryAuthorizationErrors;
  }

  ios::SigninErrorProvider* provider =
      ios::GetChromeBrowserProvider()->GetSigninErrorProvider();
  switch (provider->GetErrorCategory(error)) {
    case ios::SigninErrorCategory::UNKNOWN_ERROR: {
      // Google's OAuth 2 implementation returns a 400 with JSON body
      // containing error key "invalid_grant" to indicate the refresh token
      // is invalid or has been revoked by the user.
      // Check that the underlying library does not categorize these errors as
      // unknown.
      NSString* json_error_key = provider->GetInvalidGrantJsonErrorKey();
      DCHECK(!provider->IsBadRequest(error) ||
             ![[[error userInfo] valueForKeyPath:@"json.error"]
                 isEqual:json_error_key]);
      return kAuthenticationErrorCategoryUnknownErrors;
    }
    case ios::SigninErrorCategory::AUTHORIZATION_ERROR:
      if (provider->IsForbidden(error)) {
        return kAuthenticationErrorCategoryAuthorizationForbiddenErrors;
      }
      return kAuthenticationErrorCategoryAuthorizationErrors;
    case ios::SigninErrorCategory::NETWORK_ERROR:
      return kAuthenticationErrorCategoryNetworkServerErrors;
    case ios::SigninErrorCategory::USER_CANCELLATION_ERROR:
      return kAuthenticationErrorCategoryUserCancellationErrors;
  }
}

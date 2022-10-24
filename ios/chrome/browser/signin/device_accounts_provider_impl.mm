// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/device_accounts_provider_impl.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/constants.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the account info for `identity` (which must not be nil).
DeviceAccountsProvider::AccountInfo GetAccountInfo(
    id<SystemIdentity> identity,
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

DeviceAccountsProviderImpl::DeviceAccountsProviderImpl(
    ChromeAccountManagerService* account_manager_service)
    : account_manager_service_(account_manager_service) {
  DCHECK(account_manager_service_);
}

DeviceAccountsProviderImpl::~DeviceAccountsProviderImpl() = default;

void DeviceAccountsProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
  DCHECK(!callback.is_null());
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();

  // AccessTokenCallback is non-copyable. Using __block allocates the memory
  // directly in the block object at compilation time (instead of doing a
  // copy). This is required to have correct interaction between move-only
  // types and Objective-C blocks.
  __block AccessTokenCallback scopedCallback = std::move(callback);
  identity_service->GetAccessToken(
      account_manager_service_->GetIdentityWithGaiaID(gaia_id), client_id,
      scopes, ^(NSString* token, NSDate* expiration, NSError* error) {
        std::move(scopedCallback).Run(token, expiration, error);
      });
}

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProviderImpl::GetAllAccounts() const {
  std::vector<AccountInfo> accounts;
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  NSArray* identities = account_manager_service_->GetAllIdentities();
  for (id<SystemIdentity> identity in identities) {
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
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  if (identity_service->IsMDMError(
          account_manager_service_->GetIdentityWithGaiaID(gaia_id), error)) {
    return kAuthenticationErrorCategoryAuthorizationErrors;
  }

  switch (ios::provider::GetSigninErrorCategory(error)) {
    case ios::provider::SigninErrorCategory::kUnknownError:
      return kAuthenticationErrorCategoryUnknownErrors;
    case ios::provider::SigninErrorCategory::kAuthorizationError:
      return kAuthenticationErrorCategoryAuthorizationErrors;
    case ios::provider::SigninErrorCategory::kAuthorizationForbiddenError:
      return kAuthenticationErrorCategoryAuthorizationForbiddenErrors;
    case ios::provider::SigninErrorCategory::kNetworkError:
      return kAuthenticationErrorCategoryNetworkServerErrors;
    case ios::provider::SigninErrorCategory::kUserCancellationError:
      return kAuthenticationErrorCategoryUserCancellationErrors;
  }
}

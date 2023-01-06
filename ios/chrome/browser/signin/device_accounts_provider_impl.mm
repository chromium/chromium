// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/device_accounts_provider_impl.h"

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
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

using AccessTokenResult = DeviceAccountsProvider::AccessTokenResult;
using AccountInfo = DeviceAccountsProvider::AccountInfo;

// Helper function converting `error` for `identity` to an
// AuthenticationErrorCategory.
AuthenticationErrorCategory AuthenticationErrorCategoryFromError(
    id<SystemIdentity> identity,
    NSError* error) {
  DCHECK(error);
  if ([error.domain isEqualToString:kAuthenticationErrorDomain]) {
    if (error.code == NO_AUTHENTICATED_USER) {
      return kAuthenticationErrorCategoryUnknownIdentityErrors;
    }
  }

  ios::ChromeIdentityService* chrome_identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  if (chrome_identity_service->IsMDMError(identity, error)) {
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

  NOTREACHED() << "unexpected error: "
               << base::SysNSStringToUTF8([error description]);
}

// Helper function converting the result of fetching the access token from
// what ChromeIdentityService pass to the callback to what is expected for
// AccessTokenCallback.
AccessTokenResult AccessTokenResultFrom(id<SystemIdentity> identity,
                                        NSString* token,
                                        NSDate* expiration,
                                        NSError* error) {
  if (error) {
    return base::unexpected(
        AuthenticationErrorCategoryFromError(identity, error));
  } else {
    DCHECK(token.length);
    DeviceAccountsProvider::AccessTokenInfo info = {
        base::SysNSStringToUTF8(token),
        base::Time::FromNSDate(expiration),
    };
    return base::ok(std::move(info));
  }
}

}  // anonymous namespace

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
  id<SystemIdentity> identity =
      account_manager_service_->GetIdentityWithGaiaID(gaia_id);

  // If the identity is unknown, there is no need to try to fetch the access
  // token as it will fail immediately. Post the callback with a failure.
  if (!identity) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       base::unexpected(
                           kAuthenticationErrorCategoryUnknownIdentityErrors)));
    return;
  }

  // AccessTokenCallback is non-copyable. Using __block allocates the memory
  // directly in the block object at compilation time (instead of doing a
  // copy). This is required to have correct interaction between move-only
  // types and Objective-C blocks.
  __block AccessTokenCallback scoped_callback = std::move(callback);
  ios::GetChromeBrowserProvider().GetChromeIdentityService()->GetAccessToken(
      identity, client_id, scopes,
      ^(NSString* token, NSDate* expiration, NSError* error) {
        std::move(scoped_callback)
            .Run(AccessTokenResultFrom(identity, token, expiration, error));
      });
}

std::vector<DeviceAccountsProvider::AccountInfo>
DeviceAccountsProviderImpl::GetAllAccounts() const {
  NSArray<id<SystemIdentity>>* identities =
      account_manager_service_->GetAllIdentities();

  std::vector<AccountInfo> result;
  result.reserve(identities.count);

  ios::ChromeIdentityService* chrome_identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();

  for (id<SystemIdentity> identity : identities) {
    DCHECK(identity);
    AccountInfo account_info;
    account_info.gaia = base::SysNSStringToUTF8(identity.gaiaID);
    account_info.email = base::SysNSStringToUTF8(identity.userEmail);

    // If hosted domain is nil, then it means the information has not been
    // fetched from gaia; in that case, set account_info.hosted_domain to
    // an empty string. Otherwise, set it to the value of the hostedDomain
    // or kNoHostedDomainFound if the string is empty.
    NSString* hosted_domain =
        chrome_identity_service->GetCachedHostedDomainForIdentity(identity);
    if (hosted_domain) {
      account_info.hosted_domain = hosted_domain.length
                                       ? base::SysNSStringToUTF8(hosted_domain)
                                       : kNoHostedDomainFound;
    }
    result.push_back(std::move(account_info));
  }

  return result;
}

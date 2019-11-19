// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/signin/web_view_device_accounts_provider_impl.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/public/cwv_identity.h"
#import "ios/web_view/public/cwv_sync_controller_data_source.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebViewDeviceAccountsProviderImpl::WebViewDeviceAccountsProviderImpl() =
    default;

WebViewDeviceAccountsProviderImpl::~WebViewDeviceAccountsProviderImpl() =
    default;

void WebViewDeviceAccountsProviderImpl::GetAccessToken(
    const std::string& gaia_id,
    const std::string& client_id,
    const std::set<std::string>& scopes,
    AccessTokenCallback callback) {
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
                      .Run(access_token, expiration_date, error);
                }];
}

std::vector<DeviceAccountsProvider::AccountInfo>
WebViewDeviceAccountsProviderImpl::GetAllAccounts() const {
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

AuthenticationErrorCategory
WebViewDeviceAccountsProviderImpl::GetAuthenticationErrorCategory(
    const std::string& gaia_id,
    NSError* error) const {
  // TODO(crbug.com/780937): Implement fully.
  return kAuthenticationErrorCategoryUnknownErrors;
}

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_info_fetcher_ios.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

namespace ios {

AccountInfoFetcherIos::AccountInfoFetcherIos(
    ChromeAccountManagerService* account_manager_service,
    const GaiaId& gaia_id,
    base::OnceCallback<void(std::optional<AccountInfo>)> callback)
    : account_manager_service_(account_manager_service),
      gaia_id_(gaia_id),
      callback_(std::move(callback)) {
  CHECK(account_manager_service_);
}

AccountInfoFetcherIos::~AccountInfoFetcherIos() = default;

void AccountInfoFetcherIos::Start() {
  id<SystemIdentity> identity =
      account_manager_service_->GetIdentityWithGaiaID(gaia_id_);
  if (!identity) {
    std::move(callback_).Run(std::nullopt);
    return;
  }

  NSString* hosted_domain = GetApplicationContext()
                                ->GetSystemIdentityManager()
                                ->GetCachedHostedDomainForIdentity(identity);
  if (hosted_domain) {
    OnHostedDomainFetched(identity, hosted_domain, /*error=*/nil);
    return;
  }

  GetApplicationContext()->GetSystemIdentityManager()->GetHostedDomain(
      identity, base::BindOnce(&AccountInfoFetcherIos::OnHostedDomainFetched,
                               weak_ptr_factory_.GetWeakPtr(), identity));
}

void AccountInfoFetcherIos::OnHostedDomainFetched(id<SystemIdentity> identity,
                                                  NSString* hosted_domain,
                                                  NSError* error) {
  CHECK_EQ(gaia_id_, identity.gaiaId);

  bool has_account_info = false;
  AccountInfo::Builder builder(identity.gaiaId,
                               base::SysNSStringToUTF8(identity.userEmail));
  if (identity.userFullName.length > 0) {
    builder.SetFullName(base::SysNSStringToUTF8(identity.userFullName));
    has_account_info = true;
  }
  if (identity.userGivenName.length > 0) {
    builder.SetGivenName(base::SysNSStringToUTF8(identity.userGivenName));
    has_account_info = true;
  }
  if (hosted_domain && !error) {
    builder.SetHostedDomain(base::SysNSStringToUTF8(hosted_domain));
    has_account_info = true;
  }

  std::move(callback_).Run(
      has_account_info ? std::make_optional(builder.Build()) : std::nullopt);
}

}  // namespace ios

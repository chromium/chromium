// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_factory_ios.h"

#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_ios.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace ios {

AccountCapabilitiesFetcherFactoryIOS::AccountCapabilitiesFetcherFactoryIOS(
    ChromeAccountManagerService* account_manager_service)
    : account_manager_service_(account_manager_service) {}

AccountCapabilitiesFetcherFactoryIOS::~AccountCapabilitiesFetcherFactoryIOS() =
    default;

std::unique_ptr<AccountCapabilitiesFetcher>
AccountCapabilitiesFetcherFactoryIOS::CreateAccountCapabilitiesFetcher(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback) {
  return std::make_unique<AccountCapabilitiesFetcherIOS>(
      account_info, fetch_priority, account_manager_service_,
      std::move(on_complete_callback));
}
}  // namespace ios

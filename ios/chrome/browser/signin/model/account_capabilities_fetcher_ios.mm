// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_ios.h"

#import <map>
#import <optional>

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

namespace ios {
namespace {

// Converts the vector of string to a set of string.
std::set<std::string> SetFromVector(const std::vector<std::string>& strings) {
  return std::set<std::string>(strings.begin(), strings.end());
}

// Converts the value returned by SystemIdentityManager::FetchCapabilities()
// to the format expected from CompleteFetchAndMaybeDestroySelf().
std::optional<AccountCapabilities> AccountCapabilitiesFromCapabilitiesMap(
    std::map<std::string, SystemIdentityCapabilityResult> capabilities_map) {
  base::flat_map<std::string, bool> capabilities;
  for (const auto& pair : capabilities_map) {
    switch (pair.second) {
      case SystemIdentityCapabilityResult::kTrue:
        capabilities[pair.first] = true;
        break;

      case SystemIdentityCapabilityResult::kFalse:
        capabilities[pair.first] = false;
        break;

      case SystemIdentityCapabilityResult::kUnknown:
        break;
    }
  }
  return AccountCapabilities(std::move(capabilities));
}

}  // anonymous namespace

AccountCapabilitiesFetcherIOS::~AccountCapabilitiesFetcherIOS() = default;

AccountCapabilitiesFetcherIOS::AccountCapabilitiesFetcherIOS(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::FetchPriority fetch_priority,
    ChromeAccountManagerService* account_manager_service,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback)
    : AccountCapabilitiesFetcher(account_info,
                                 fetch_priority,
                                 std::move(on_complete_callback)),
      account_manager_service_(account_manager_service) {}

void AccountCapabilitiesFetcherIOS::StartImpl() {
  id<SystemIdentity> identity =
      account_manager_service_->GetIdentityWithGaiaID(account_info().gaia);

  // If the `account_manager_service_` and system identity manager are out of
  // sync the `identity` may not have been written yet to the latter. In this
  // case do not fetch capabilities.
  if (!identity) {
    return;
  }

  auto callback =
      base::BindOnce(&AccountCapabilitiesFromCapabilitiesMap)
          .Then(base::BindOnce(
              &AccountCapabilitiesFetcherIOS::CompleteFetchAndMaybeDestroySelf,
              weak_ptr_factory_.GetWeakPtr()));

  GetApplicationContext()->GetSystemIdentityManager()->FetchCapabilities(
      identity,
      SetFromVector(AccountCapabilities::GetSupportedAccountCapabilityNames()),
      std::move(callback));
}

}  // namespace ios

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/account_capabilities_fetcher_ios.h"

#import <map>
#import <optional>

#import "base/containers/to_vector.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"

namespace ios {
namespace {

// Converts the value returned by SystemIdentityManager::FetchCapabilities()
// to the format expected from the fetch callbacks.
AccountCapabilities AccountCapabilitiesFromCapabilitiesMap(
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
    AccountCapabilitiesFetcher::OnSomeCapabilitiesFetchedCallback
        on_some_capabilities_fetched_callback,
    AccountCapabilitiesFetcher::OnAllFetchesCompleteCallback
        on_all_fetches_complete_callback)
    : AccountCapabilitiesFetcher(
          account_info,
          fetch_priority,
          std::move(on_some_capabilities_fetched_callback),
          std::move(on_all_fetches_complete_callback)),
      account_manager_service_(account_manager_service) {}

void AccountCapabilitiesFetcherIOS::StartImpl() {
  id<SystemIdentity> identity =
      account_manager_service_->GetIdentityOnDeviceWithGaiaID(
          account_info().gaia);

  // If the `account_manager_service_` and system identity manager are out of
  // sync the `identity` may not have been written yet to the latter. In this
  // case do not fetch capabilities.
  if (!identity) {
    return;
  }

  std::vector<std::string> capability_names =
      base::ToVector(AccountCapabilities::GetSupportedAccountCapabilityNames(),
                     [](std::string_view sv) { return std::string(sv); });

  if (base::FeatureList::IsEnabled(switches::kBuildExternalPrivacyContext)) {
    auto partial_callback =
        base::BindRepeating(&AccountCapabilitiesFromCapabilitiesMap)
            .Then(base::BindRepeating(
                &AccountCapabilitiesFetcherIOS::UpdateFetchedCapabilities,
                weak_ptr_factory_.GetWeakPtr()));

    auto completion_callback = base::BindOnce(
        &AccountCapabilitiesFetcherIOS::CompleteFetchAndMaybeDestroySelf,
        weak_ptr_factory_.GetWeakPtr());

    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->FetchCapabilitiesWithPartial(identity, capability_names,
                                       std::move(completion_callback),
                                       std::move(partial_callback));
  } else {
    auto callback =
        base::BindOnce(&AccountCapabilitiesFromCapabilitiesMap)
            .Then(base::BindOnce(&AccountCapabilitiesFetcherIOS::
                                     UpdateAndCompleteFetchAndMaybeDestroySelf,
                                 weak_ptr_factory_.GetWeakPtr()));

    GetApplicationContext()->GetSystemIdentityManager()->FetchCapabilities(
        identity, capability_names, std::move(callback));
  }
}

}  // namespace ios

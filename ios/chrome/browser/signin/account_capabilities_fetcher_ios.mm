// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import <map>

#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

// Converts the vector of string to a set of string.
std::set<std::string> SetFromVector(const std::vector<std::string>& strings) {
  return std::set<std::string>(strings.begin(), strings.end());
}

// Converts the value returned by SystemIdentityManager::FetchCapabilities()
// to the format expected from CompleteFetchAndMaybeDestroySelf().
absl::optional<AccountCapabilities> AccountCapabilitiesFromCapabilitiesMap(
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
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback,
    id<SystemIdentity> system_identity)
    : AccountCapabilitiesFetcher(account_info, std::move(on_complete_callback)),
      system_identity_(system_identity) {}

void AccountCapabilitiesFetcherIOS::StartImpl() {
  auto callback =
      base::BindOnce(&AccountCapabilitiesFromCapabilitiesMap)
          .Then(base::BindOnce(
              &AccountCapabilitiesFetcherIOS::CompleteFetchAndMaybeDestroySelf,
              weak_ptr_factory_.GetWeakPtr()));

  GetApplicationContext()->GetSystemIdentityManager()->FetchCapabilities(
      system_identity_,
      SetFromVector(AccountCapabilities::GetSupportedAccountCapabilityNames()),
      std::move(callback));
}

}  // namespace ios

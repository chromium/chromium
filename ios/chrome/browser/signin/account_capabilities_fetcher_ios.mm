// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/strings/sys_string_conversions.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

AccountCapabilitiesFetcherIOS::~AccountCapabilitiesFetcherIOS() = default;

AccountCapabilitiesFetcherIOS::AccountCapabilitiesFetcherIOS(
    const CoreAccountInfo& account_info,
    AccountCapabilitiesFetcher::OnCompleteCallback on_complete_callback,
    ios::ChromeIdentityService* chrome_identity_service,
    id<SystemIdentity> system_identity)
    : AccountCapabilitiesFetcher(account_info, std::move(on_complete_callback)),
      chrome_identity_service_(chrome_identity_service),
      system_identity_(system_identity) {}

void AccountCapabilitiesFetcherIOS::StartImpl() {
  const std::vector<std::string> capability_name_vector =
      AccountCapabilities::GetSupportedAccountCapabilityNames();
  NSMutableArray<NSString*>* capability_name_array;
  for (const std::string& capability : capability_name_vector) {
    [capability_name_array addObject:base::SysUTF8ToNSString(capability)];
  }

  __block auto callback =
      base::BindOnce(&AccountCapabilitiesFetcherIOS::OnCapabilitiesFetched,
                     weak_ptr_factory_.GetWeakPtr());
  chrome_identity_service_->FetchCapabilities(
      system_identity_, capability_name_array,
      ^(CapabilitiesDict* dict, NSError* err) {
        std::move(callback).Run(dict, err);
      });
}

void AccountCapabilitiesFetcherIOS::OnCapabilitiesFetched(
    CapabilitiesDict* dict,
    NSError* err) {
  __block base::flat_map<std::string, bool> capabilities_map;
  [dict enumerateKeysAndObjectsUsingBlock:^(NSString* name, NSNumber* value,
                                            BOOL* stop) {
    ios::ChromeIdentityCapabilityResult capability_state =
        static_cast<ios::ChromeIdentityCapabilityResult>(value.intValue);
    switch (value.intValue) {
      case static_cast<int>(ios::ChromeIdentityCapabilityResult::kTrue):
      case static_cast<int>(ios::ChromeIdentityCapabilityResult::kFalse):
        capabilities_map[base::SysNSStringToUTF8(name)] =
            capability_state == ios::ChromeIdentityCapabilityResult::kTrue;
        break;
      case static_cast<int>(ios::ChromeIdentityCapabilityResult::kUnknown):
        break;
      default:
        NOTREACHED();
    }
  }];
  absl::optional<AccountCapabilities> account_capabilities =
      AccountCapabilities(std::move(capabilities_map));
  CompleteFetchAndMaybeDestroySelf(account_capabilities);
}

}  // namespace ios

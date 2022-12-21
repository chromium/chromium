// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/account_capabilities_fetcher_ios.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace {

// Converts the vector of string to an NSArray of NSString.
NSArray<NSString*>* ArrayFromVector(const std::vector<std::string>& strings) {
  NSMutableArray<NSString*>* result = [[NSMutableArray alloc] init];
  for (const std::string& string : strings) {
    [result addObject:base::SysUTF8ToNSString(string)];
  }
  return result;
}

// Converts the value returned by SystemIdentityManager::FetchCapabilities()
// to the format expected from CompleteFetchAndMaybeDestroySelf().
absl::optional<AccountCapabilities> AccountCapabilitiesFromCapabilitiesDict(
    CapabilitiesDict* capabilities_dict,
    NSError* error) {
  base::flat_map<std::string, bool> capabilities;
  for (NSString* key in capabilities_dict) {
    const int value = [capabilities_dict[key] intValue];
    switch (value) {
      case static_cast<int>(SystemIdentityCapabilityResult::kTrue):
        capabilities[base::SysNSStringToUTF8(key)] = true;
        break;

      case static_cast<int>(SystemIdentityCapabilityResult::kFalse):
        capabilities[base::SysNSStringToUTF8(key)] = false;
        break;

      case static_cast<int>(SystemIdentityCapabilityResult::kUnknown):
        break;

      default:
        NOTREACHED() << "unknown capability value: " << value;
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
  __block auto callback = base::BindOnce(
      &AccountCapabilitiesFetcherIOS::CompleteFetchAndMaybeDestroySelf,
      weak_ptr_factory_.GetWeakPtr());

  ios::GetChromeBrowserProvider().GetChromeIdentityService()->FetchCapabilities(
      system_identity_,
      ArrayFromVector(
          AccountCapabilities::GetSupportedAccountCapabilityNames()),
      ^(CapabilitiesDict* dict, NSError* error) {
        std::move(callback).Run(
            AccountCapabilitiesFromCapabilitiesDict(dict, error));
      });
}

}  // namespace ios

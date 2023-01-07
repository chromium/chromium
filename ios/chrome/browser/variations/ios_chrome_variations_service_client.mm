// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/ios_chrome_variations_service_client.h"

#import "base/strings/sys_string_conversions.h"
#import "base/version.h"
#import "components/variations/seed_response.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/variations/ios_chrome_first_run_variations_seed_manager.h"
#import "ios/chrome/browser/variations/ios_chrome_seed_response.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeVariationsServiceClient::IOSChromeVariationsServiceClient() = default;

IOSChromeVariationsServiceClient::~IOSChromeVariationsServiceClient() = default;

base::Version IOSChromeVariationsServiceClient::GetVersionForSimulation() {
  // TODO(crbug.com/1288101): Get the version that will be used on restart
  // instead of the current version.
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeVariationsServiceClient::GetURLLoaderFactory() {
  return GetApplicationContext()->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker*
IOSChromeVariationsServiceClient::GetNetworkTimeTracker() {
  return GetApplicationContext()->GetNetworkTimeTracker();
}

version_info::Channel IOSChromeVariationsServiceClient::GetChannel() {
  return ::GetChannel();
}

bool IOSChromeVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
  return false;
}

bool IOSChromeVariationsServiceClient::IsEnterprise() {
  // TODO(crbug.com/1003846): Implement enterprise check for iOS.
  return false;
}

std::unique_ptr<variations::SeedResponse>
IOSChromeVariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
  IOSChromeFirstRunVariationsSeedManager* mgr =
      [[IOSChromeFirstRunVariationsSeedManager alloc] init];
  IOSChromeSeedResponse* ios_seed = [mgr popSeed];
  if (!ios_seed) {
    return nullptr;
  }

  auto seed = std::make_unique<variations::SeedResponse>();
  if (ios_seed.data != nil) {
    NSString* base64EncodedNSString =
        [ios_seed.data base64EncodedStringWithOptions:0];
    seed->data = base::SysNSStringToUTF8(base64EncodedNSString);
  }
  seed->signature = base::SysNSStringToUTF8(ios_seed.signature);
  seed->country = base::SysNSStringToUTF8(ios_seed.country);
  seed->date = ios_seed.time.timeIntervalSince1970;
  seed->is_gzip_compressed = ios_seed.compressed;
  return seed;
}

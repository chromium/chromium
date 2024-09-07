// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/plus_addresses/fake_plus_address_service.h"
#import "components/plus_addresses/plus_address_test_utils.h"
#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {

plus_addresses::FakePlusAddressService* GetFakePlusAddressService() {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  return static_cast<plus_addresses::FakePlusAddressService*>(
      PlusAddressServiceFactory::GetForProfile(browserState));
}

}  // namespace

@implementation PlusAddressAppInterface

+ (void)setShouldOfferPlusAddressCreation:(BOOL)shouldOfferPlusAddressCreation {
  GetFakePlusAddressService()->set_should_offer_plus_address_creation(
      shouldOfferPlusAddressCreation);
}

+ (void)setShouldReturnNoAffiliatedPlusProfiles:
    (BOOL)shouldReturnNoAffiliatedPlusProfiles {
  GetFakePlusAddressService()->set_should_return_no_affiliated_plus_profiles(
      shouldReturnNoAffiliatedPlusProfiles);
}

+ (void)setPlusAddressFillingEnabled:(BOOL)plusAddressFillingEnabled {
  GetFakePlusAddressService()->set_is_plus_address_filling_enabled(
      plusAddressFillingEnabled);
}

+ (void)addPlusAddressProfile {
  GetFakePlusAddressService()->add_plus_profile(
      plus_addresses::test::CreatePlusProfile());
}

@end

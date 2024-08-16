// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/plus_addresses/ui/plus_address_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/plus_addresses/plus_address_service.h"
#import "components/plus_addresses/plus_address_test_utils.h"
#import "components/plus_addresses/plus_address_types.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation PlusAddressAppInterface

+ (void)saveExamplePlusProfile:(NSString*)url {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  plus_addresses::PlusAddressService* plusAddressService =
      PlusAddressServiceFactory::GetForBrowserState(browserState);

  plusAddressService->SavePlusProfile(plus_addresses::PlusProfile(
      /*profile_id=*/"234", base::SysNSStringToUTF8(url),
      plus_addresses::PlusAddress(plus_addresses::test::kFakePlusAddress),
      /*is_confirmed=*/true));
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/phone_number/ui_bundled/country_code_picker_app_interface.h"

#import "ios/chrome/browser/shared/public/commands/country_code_picker_commands.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation CountryCodePickerAppInterface

namespace {
NSString* phoneNumber = @"0666666666";
}

+ (void)presentCountryCodePicker {
  id<CountryCodePickerCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler presentCountryCodePickerForPhoneNumber:phoneNumber];
}

+ (void)stopPresentingCountryCodePicker {
  id<CountryCodePickerCommands> handler =
      chrome_test_util::HandlerForActiveBrowser();
  [handler hideCountryCodePicker];
}

@end

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COUNTRY_CODE_PICKER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COUNTRY_CODE_PICKER_COMMANDS_H_

// Commands related to the country code.
@protocol CountryCodePickerCommands <NSObject>

// Shows the `CountryCodePicker` view for a given phone number.
- (void)presentCountryCodePickerForPhoneNumber:(NSString*)phoneNumber;

// Hides the `CountryCodePicker`view.
- (void)hideCountryCodePicker;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_COUNTRY_CODE_PICKER_COMMANDS_H_

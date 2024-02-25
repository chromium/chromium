// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PHONE_NUMBER_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PHONE_NUMBER_COMMANDS_H_

// Commands related to the Phone Number Experience.
@protocol PhoneNumberCommands <NSObject>

// Shows the `Add to Contacts` view for a given phone number.
- (void)presentAddContactsForPhoneNumber:(NSString*)phoneNumber;

// Hides the `Add to Contacts`view.
- (void)hideAddContacts;

// TODO(crbug.com/1500879): Add the functions to present and hide the country
// code view.
@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_PHONE_NUMBER_COMMANDS_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSCODE_SETTINGS_PASSCODE_SETTINGS_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSCODE_SETTINGS_PASSCODE_SETTINGS_API_H_

#import <Foundation/Foundation.h>

namespace ios::provider {

// Returns whether Passcode Settings is enabled for Chrome.
BOOL SupportsPasscodeSettings();

// Opens passcode settings programmatically.
void OpenPasscodeSettings();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSCODE_SETTINGS_PASSCODE_SETTINGS_API_H_

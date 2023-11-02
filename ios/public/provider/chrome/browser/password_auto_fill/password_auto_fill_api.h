// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSWORD_AUTO_FILL_PASSWORD_AUTO_FILL_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSWORD_AUTO_FILL_PASSWORD_AUTO_FILL_API_H_

#import <Foundation/Foundation.h>

namespace ios {
namespace provider {

// Returns whether shortened instructions to enable auto-fill is now supported
// by Chrome.
BOOL SupportShortenedInstructionForPasswordAutoFill();

// Opens settings programmatically, invoked by Passwords In Other Apps view
// controller.
void PasswordsInOtherAppsOpensSettings();

}  // namespace provider
}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PASSWORD_AUTO_FILL_PASSWORD_AUTO_FILL_API_H_

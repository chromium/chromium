// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_CONSTANTS_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_CONSTANTS_H_

#import <Foundation/Foundation.h>

namespace ios {

// Email suffix used for managed accounts.
extern NSString* const kManagedIdentityEmailSuffix;

// Argument to use when starting FakeChromeIdentityService with a list of
// FakeSystemIdentity. The value is a NSArray of FakeSystemIdentity encoded. See
// +[FakeSystemIdentity encodeIdentitiesToBase64:].
extern const char* const kAddFakeIdentitiesArg;

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_FAKE_CHROME_IDENTITY_SERVICE_CONSTANTS_H_

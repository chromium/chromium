// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_

#include "ios/chrome/browser/signin/system_identity.h"

// A single identity used for signing in.
// Identity is the equivalent of an account. A user may have multiple identities
// or accounts associated with a single device.
@interface ChromeIdentity : NSObject <SystemIdentity>

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_SIGNIN_CHROME_IDENTITY_H_

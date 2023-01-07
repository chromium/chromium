// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {

NSString* const kManagedIdentityEmailSuffix = @"@google.com";

NSString* const kManagedExampleIdentityEmailSuffix = @"@example.com";

const char* const kAddFakeIdentitiesArg = "add_fake_identities";

NSArray<NSString*>* GetManagedEmailSuffixes() {
  return @[ kManagedIdentityEmailSuffix, kManagedExampleIdentityEmailSuffix ];
}

}  // namespace ios

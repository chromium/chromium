// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/test_switches.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace test_switches {

// Uses the real SystemIdentityManager for EarlGrey tests.
const char kForceRealSystemIdentityManager[] =
    "force-real-system-identity-manager";

// Identities to add at startup.
const char kAddFakeIdentitiesAtStartup[] = "add_fake_identities";

// Sign in automatically at startup.
const char kSignInAtStartup[] = "sign-in-at-startup";

}  // namespace test_switches

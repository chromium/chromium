// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_
#define IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_

namespace test_switches {

// Switch used to force the use of the real SystemIdentityManager for EarlGrey
// tests (i.e. the one returned by the provider API). If not specified, a fake
// SystemIdentityManager is installed for all EarlGrey tests.
extern const char kForceRealSystemIdentityManager[];

// Switch used to add identities when starting the fake SystemIdentityManager.
// The value comes from `+[FakeSystemIdentity encodeIdentitiesToBase64:]`.
//
// Ignored if kForceRealSystemIdentityManager is used.
extern const char kAddFakeIdentitiesAtStartup[];

// Switch used to record an identity at startup to avoid automatic sign out.
// Only uses the identities from the `kAddFakeIdentitiesAtStartup` switch if
// the switch is set, otherwise` fakeIdentity1` is used by default.
//
// Ignored if kForceRealSystemIdentityManager is used.
extern const char kSignInAtStartup[];

// Switch used to enable FakeTabGroupSyncService for testing. The feature
// kTabGroupSync should be enabled as well.
extern const char kEnableFakeTabGroupSyncService[];

// Switch used to force the status of fetching the Google Family for the user in
// Earl Grey tests. If not specified, unknown status is retuned for all tests.
extern const char kFamilyStatus[];

// Switch used to enable a certain IPH for testing. The value should be the IPH
// feature to enable. Leave blank/empty value to enable all IPH in testing.
extern const char kEnableIPH[];

}  // namespace test_switches

#endif  // IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_

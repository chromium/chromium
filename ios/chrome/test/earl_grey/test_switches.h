// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_
#define IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_

namespace test_switches {

// Switch used to record an identity at startup to avoid automatic sign out.
// Only uses the identities from the `ios::kAddFakeIdentitiesArg` switch if the
// switch is set, otherwise fakeIdentity1 is used by default.
extern const char kSignInAtStartup[];

}  // namespace test_switches

#endif  // IOS_CHROME_TEST_EARL_GREY_TEST_SWITCHES_H_

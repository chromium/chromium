// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/test_switches.h"

namespace test_switches {

// Uses the real SystemIdentityManager for EarlGrey tests.
const char kForceRealSystemIdentityManager[] =
    "force-real-system-identity-manager";

// Identities to add at startup.
const char kAddFakeIdentitiesAtStartup[] = "add-fake-identities";

// Simulate post device restore.
const char kSimulatePostDeviceRestore[] = "simulate-post-device-restore";

// Enables FakeTabGroupSyncService.
const char kEnableFakeTabGroupSyncService[] =
    "enable-fake-tab-group-sync-service";

// Status of the Google Family fetch API call for the user.
const char kFamilyStatus[] = "family-status";

// Enables the provided IPH.
const char kEnableIPH[] = "enable-iph";

// Installs a mock ShoppingService.
const char kMockShoppingService[] = "mock-shopping-service";

// Indicates that the test will run with minimal UI.
const char kLoadMinimalAppUI[] = "load-minimal-app-ui";

}  // namespace test_switches

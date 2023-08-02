// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/flags/about_flags.h"

#import "components/flags_ui/feature_entry.h"
#import "components/flags_ui/flags_test_helpers.h"
#import "testing/platform_test.h"

using AboutFlagsTest = PlatformTest;

// Makes sure that every flag has an owner and an expiry entry in
// flag-metadata.json.
TEST_F(AboutFlagsTest, EveryFlagHasMetadata) {
  flags_ui::testing::EnsureEveryFlagHasMetadata(testing::GetFeatureEntries());
}

// Ensures that all flags marked as never expiring in flag-metadata.json is
// listed in flag-never-expire-list.json.
TEST_F(AboutFlagsTest, OnlyPermittedFlagsNeverExpire) {
  flags_ui::testing::EnsureOnlyPermittedFlagsNeverExpire();
}

// Ensures that every flag has an owner.
TEST_F(AboutFlagsTest, EveryFlagHasNonEmptyOwners) {
  flags_ui::testing::EnsureEveryFlagHasNonEmptyOwners();
}

// Ensures that owners conform to rules in flag-metadata.json.
TEST_F(AboutFlagsTest, OwnersLookValid) {
  flags_ui::testing::EnsureOwnersLookValid();
}

// Ensures that flags are listed in alphabetical order in flag-metadata.json and
// flag-never-expire-list.json.
TEST_F(AboutFlagsTest, FlagsListedInAlphabeticalOrder) {
  flags_ui::testing::EnsureFlagsAreListedInAlphabeticalOrder();
}

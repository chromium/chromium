// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_util.h"

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/location_bar/badge/model/badge_type.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using LocationBarBadgeUtilTest = PlatformTest;

// Tests that supported `InfobarType` values map to `LocationBarBadgeType`.
TEST_F(LocationBarBadgeUtilTest, ReturnsBadgeTypeWhenInfobarTypeIsSupported) {
  EXPECT_EQ(
      LocationBarBadgeType::kTranslate,
      LocationBarBadgeTypeForInfobarType(InfobarType::kInfobarTypeTranslate));
  EXPECT_EQ(
      LocationBarBadgeType::kPermissionsCamera,
      LocationBarBadgeTypeForInfobarType(InfobarType::kInfobarTypePermissions));
  EXPECT_EQ(
      LocationBarBadgeType::kReaderMode,
      LocationBarBadgeTypeForInfobarType(InfobarType::kInfobarTypeReaderMode));
}

// Tests that unsupported, password, and autofill `InfobarType` values return
// `std::nullopt`.
TEST_F(LocationBarBadgeUtilTest, ReturnsNulloptWhenInfobarTypeIsUnsupported) {
  for (InfobarType type : {
           InfobarType::kInfobarTypeConfirm,
           InfobarType::kInfobarTypeSignin,
           InfobarType::kInfobarTypeSyncError,
           InfobarType::kInfobarTypePasswordSave,
           InfobarType::kInfobarTypePasswordUpdate,
           InfobarType::kInfobarTypeSaveCard,
           InfobarType::kInfobarTypeSaveAutofillAddressProfile,
       }) {
    EXPECT_EQ(std::nullopt, LocationBarBadgeTypeForInfobarType(type));
  }
}

// Tests that `IsContextualPanelEntryPointBadgeType` returns true only for
// contextual panel entry point badge types.
TEST_F(LocationBarBadgeUtilTest,
       ReturnsExpectedResultForContextualPanelEntryPointBadgeTypes) {
  EXPECT_TRUE(
      IsContextualPanelEntryPointBadgeType(LocationBarBadgeType::kReaderMode));
  EXPECT_TRUE(IsContextualPanelEntryPointBadgeType(
      LocationBarBadgeType::kContextualPanelEntryPointSample));
  EXPECT_FALSE(
      IsContextualPanelEntryPointBadgeType(LocationBarBadgeType::kNone));
  EXPECT_FALSE(
      IsContextualPanelEntryPointBadgeType(LocationBarBadgeType::kTranslate));
  EXPECT_FALSE(IsContextualPanelEntryPointBadgeType(
      LocationBarBadgeType::kGeminiContextualCueChip));
}

// Tests that `IsBadgeFromFactory` returns true only for badge types originating
// from the badge factory.
TEST_F(LocationBarBadgeUtilTest,
       ReturnsExpectedResultForBadgeTypesOriginatingFromFactory) {
  for (LocationBarBadgeType type : {
           LocationBarBadgeType::kTranslate,
           LocationBarBadgeType::kPasswordSave,
           LocationBarBadgeType::kPermissionsCamera,
           LocationBarBadgeType::kPermissionsMicrophone,
           LocationBarBadgeType::kIncognito,
           LocationBarBadgeType::kOverflow,
       }) {
    EXPECT_TRUE(IsBadgeFromFactory(type));
  }
  for (LocationBarBadgeType type : {
           LocationBarBadgeType::kNone,
           LocationBarBadgeType::kContextualPanelEntryPointSample,
           LocationBarBadgeType::kReaderMode,
           LocationBarBadgeType::kGeminiContextualCueChip,
       }) {
    EXPECT_FALSE(IsBadgeFromFactory(type));
  }
}

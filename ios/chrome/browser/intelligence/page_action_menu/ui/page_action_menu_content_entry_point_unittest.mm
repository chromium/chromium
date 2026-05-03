// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_constants.h"
#import "ios/chrome/browser/intelligence/page_action_menu/utils/ai_hub_metrics.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using PageActionMenuContentEntryPointTest = PlatformTest;

// Tests the initialization of PageActionMenuContentEntryPoint.
TEST_F(PageActionMenuContentEntryPointTest, Initialization) {
  PageActionMenuContentEntryPoint* entry_point =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:YES];
  EXPECT_TRUE(entry_point.enabled);
  EXPECT_EQ(entry_point.unavailabilityItem, nil);

  entry_point = [[PageActionMenuContentEntryPoint alloc] initWithEnabled:NO];
  EXPECT_FALSE(entry_point.enabled);
  EXPECT_EQ(entry_point.unavailabilityItem, nil);
}

// Tests the initialization of PageActionMenuContentEntryPoint with a footer
// item.
TEST_F(PageActionMenuContentEntryPointTest, InitializationWithFooterItem) {
  ContentEntryPointUnavailabilityItem* item =
      [ContentEntryPointUnavailabilityItem geminiEnterprise];
  PageActionMenuContentEntryPoint* entry_point =
      [[PageActionMenuContentEntryPoint alloc] initWithEnabled:NO
                                                    footerItem:item];
  EXPECT_FALSE(entry_point.enabled);
  EXPECT_EQ(entry_point.unavailabilityItem, item);
}

// Tests the geminiEnterprise factory method.
TEST_F(PageActionMenuContentEntryPointTest, GeminiEnterpriseFactory) {
  ContentEntryPointUnavailabilityItem* item =
      [ContentEntryPointUnavailabilityItem geminiEnterprise];
  EXPECT_NE(item, nil);
  EXPECT_TRUE([item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_AI_HUB_GEMINI_UNAVAILABLE_ENTERPRISE_LABEL)]);
  EXPECT_NE(item.icon, nil);
  EXPECT_EQ(item.actionIdentifier, nil);
  EXPECT_EQ(item.metricIdentifier,
            IOSPageActionMenuFooterReason::kGeminiEnterprise);
}

// Tests the lensEnterprise factory method.
TEST_F(PageActionMenuContentEntryPointTest, LensEnterpriseFactory) {
  ContentEntryPointUnavailabilityItem* item =
      [ContentEntryPointUnavailabilityItem lensEnterprise];
  EXPECT_NE(item, nil);
  EXPECT_TRUE([item.text
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_AI_HUB_LENS_UNAVAILABLE_ENTERPRISE_LABEL)]);
  EXPECT_NE(item.icon, nil);
  EXPECT_EQ(item.actionIdentifier, nil);
  EXPECT_EQ(item.metricIdentifier,
            IOSPageActionMenuFooterReason::kLensEnterprise);
}

// Tests the lensSearchEngine factory method.
TEST_F(PageActionMenuContentEntryPointTest, LensSearchEngineFactory) {
  ContentEntryPointUnavailabilityItem* item =
      [ContentEntryPointUnavailabilityItem lensSearchEngine];
  EXPECT_NE(item, nil);
  EXPECT_TRUE(
      [item.text isEqualToString:
                     l10n_util::GetNSString(
                         IDS_IOS_AI_HUB_LENS_UNAVAILABLE_SEARCH_ENGINE_LABEL)]);
  EXPECT_EQ(item.icon, nil);
  EXPECT_TRUE([item.actionIdentifier
      isEqualToString:kSearchEngineSettingsActionIdentifier]);
  EXPECT_EQ(item.metricIdentifier,
            IOSPageActionMenuFooterReason::kLensSearchEngine);
}

}  // namespace

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_data_source.h"

#import "base/base_paths.h"
#import "base/files/file_util.h"
#import "base/path_service.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/gtest_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

using WhatsNewDataSourceTest = PlatformTest;

base::FilePath GetTestDataPath() {
  base::FilePath test_path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_path));
  test_path = test_path.Append(FILE_PATH_LITERAL("whats_new"));
  test_path = test_path.Append(FILE_PATH_LITERAL("test"));
  test_path = test_path.Append(FILE_PATH_LITERAL("data"));
  test_path = test_path.Append(FILE_PATH_LITERAL("whats_new_test.plist"));
  return test_path;
}

}  // namespace

// Test that WhatsNewItem is constructed correctly from a valid entry in the
// test file.
TEST_F(WhatsNewDataSourceTest, TestConstructionOfWhatsNewItem) {
  base::FilePath test_path;
  test_path = GetTestDataPath();
  NSString* path = base::SysUTF8ToNSString(test_path.value().c_str());

  NSDictionary* entries = [NSDictionary dictionaryWithContentsOfFile:path];
  NSArray<NSDictionary*>* keys = [entries objectForKey:@"WhatsNewItemTestKey"];
  NSDictionary* entry = [keys objectAtIndex:0];

  WhatsNewItem* item = ConstructWhatsNewItem(entry);
  EXPECT_EQ(item.type, WhatsNewType::kNewOverflowMenu);
  EXPECT_NSEQ(item.title,
              l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_NEW_MENU_TITLE));
  EXPECT_NSEQ(item.subtitle, l10n_util::GetNSString(
                                 IDS_IOS_WHATS_NEW_FEATURE_NEW_MENU_SUBTITLE));
  EXPECT_NSEQ(
      item.instructionSteps[0],
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_NEW_MENU_STEP_1));
  EXPECT_NSEQ(
      item.instructionSteps[1],
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_NEW_MENU_STEP_2));
  EXPECT_EQ(item.primaryActionTitle, nil);
}

// Test that WhatsNewItem is not constructed when the entry in the test file has
// an invalid type.
TEST_F(WhatsNewDataSourceTest, TestWhatsNewItemEntryError) {
  base::FilePath test_path;
  test_path = GetTestDataPath();
  NSString* path = base::SysUTF8ToNSString(test_path.value().c_str());

  NSDictionary* entries = [NSDictionary dictionaryWithContentsOfFile:path];
  NSArray<NSDictionary*>* keys =
      [entries objectForKey:@"WhatsNewItemTestKeyError"];
  NSDictionary* entry = [keys objectAtIndex:0];

  EXPECT_CHECK_DEATH(ConstructWhatsNewItem(entry));
}

// Test that `WhatsNewFeatureEntries` correctly returns entries Features entry
TEST_F(WhatsNewDataSourceTest, TestFeatureEntries) {
  base::FilePath test_path;
  test_path = GetTestDataPath();
  NSString* path = base::SysUTF8ToNSString(test_path.value().c_str());
  NSArray<WhatsNewItem*>* features = WhatsNewFeatureEntries(path);

  WhatsNewItem* item = [features objectAtIndex:0];
  EXPECT_EQ(item.type, WhatsNewType::kSearchTabs);
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_WHATS_NEW_FEATURE_SEARCH_TABS_TITLE));
  EXPECT_NSEQ(
      item.subtitle,
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_SEARCH_TABS_SUBTITLE));
  EXPECT_NSEQ(
      item.instructionSteps[0],
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_SEARCH_TABS_STEP_1));
  EXPECT_NSEQ(
      item.instructionSteps[1],
      l10n_util::GetNSString(IDS_IOS_WHATS_NEW_FEATURE_SEARCH_TABS_STEP_2));
  EXPECT_EQ(item.primaryActionTitle, nil);
}

// Test that `WhatsNewChromeTipEntries` correctly returns ChromeTips entry.
TEST_F(WhatsNewDataSourceTest, TestChromeTipEntries) {
  base::FilePath test_path;
  test_path = GetTestDataPath();
  NSString* path = base::SysUTF8ToNSString(test_path.value().c_str());
  NSArray<WhatsNewItem*>* chrome_tips = WhatsNewChromeTipEntries(path);

  WhatsNewItem* item = [chrome_tips objectAtIndex:0];
  EXPECT_EQ(item.type, WhatsNewType::kUseChromeByDefault);
  EXPECT_NSEQ(item.title, l10n_util::GetNSString(
                              IDS_IOS_WHATS_CHROME_TIP_CHROME_DEFAULT_TITLE));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_CHROME_TIP_CHROME_DEFAULT_SUBTITLE));
  EXPECT_NSEQ(item.instructionSteps[0],
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_CHROME_TIP_CHROME_DEFAULT_STEP_1));
  EXPECT_NSEQ(item.instructionSteps[1],
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_CHROME_TIP_CHROME_DEFAULT_STEP_2));
  EXPECT_NSEQ(item.primaryActionTitle,
              l10n_util::GetNSString(IDS_IOS_OPEN_IOS_SETTINGS));
}

// Test that WhatsNewItem M116 is constructed correctly from a valid entry in
// the test file.
TEST_F(WhatsNewDataSourceTest, TestConstructionOfWhatsNewItemM116) {
  base::test::ScopedFeatureList features;
  base::FilePath test_path;
  test_path = GetTestDataPath();
  NSString* path = base::SysUTF8ToNSString(test_path.value().c_str());

  NSDictionary* entries = [NSDictionary dictionaryWithContentsOfFile:path];
  NSArray<NSDictionary*>* keys =
      [entries objectForKey:@"WhatsNewItemM116TestKey"];
  NSDictionary* entry = [keys objectAtIndex:0];

  WhatsNewItem* item = ConstructWhatsNewItem(entry);
  EXPECT_EQ(item.type, WhatsNewType::kIncognitoTabsFromOtherApps);
  EXPECT_NSEQ(item.title,
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_TITLE));
  EXPECT_NSEQ(item.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_SUBTITLE));
  EXPECT_NSEQ(item.screenshotName, @"incognito_tabs_from_other_apps");
  NSDictionary* image_text_expected = @{
    @"IDS_OPEN_IN_INCOGNITO" : l10n_util::GetNSString(
        IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_SCREENSHOT_TEXT_OPEN_IN_INCOGNITO),
    @"IDS_OPEN_IN_INCOGNITO_URL" : l10n_util::GetNSString(
        IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_SCREENSHOT_TEXT_URL)
  };
  EXPECT_TRUE(
      [item.screenshotTextProvider isEqualToDictionary:image_text_expected]);
  EXPECT_NSEQ(item.instructionSteps[0],
              l10n_util::GetNSString(IDS_IOS_WHATS_NEW_APP_SETTINGS));
  EXPECT_NSEQ(item.instructionSteps[1],
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_STEP_2));
  EXPECT_NSEQ(item.instructionSteps[2],
              l10n_util::GetNSString(
                  IDS_IOS_WHATS_NEW_INCOGNITO_TABS_FROM_OTHER_APPS_STEP_3));
  EXPECT_NSEQ(item.primaryActionTitle,
              l10n_util::GetNSString(IDS_IOS_OPEN_IOS_SETTINGS));
}

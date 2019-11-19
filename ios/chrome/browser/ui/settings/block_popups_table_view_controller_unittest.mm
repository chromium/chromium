// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/ui/settings/block_popups_table_view_controller.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SettingsRootTableViewController (ExposedForTesting)
- (void)editButtonPressed;
@end

namespace {

const char* kAllowedPattern = "[*.]example.com";
const char* kAllowedURL = "http://example.com";

class BlockPopupsTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
  }

  ChromeTableViewController* InstantiateController() override {
    return [[BlockPopupsTableViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  void SetDisallowPopups() {
    ios::HostContentSettingsMapFactory::GetForBrowserState(
        chrome_browser_state_.get())
        ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_BLOCK);
  }

  void SetAllowPopups() {
    ios::HostContentSettingsMapFactory::GetForBrowserState(
        chrome_browser_state_.get())
        ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_ALLOW);
  }

  void AddAllowedPattern(const std::string& pattern, const GURL& url) {
    ContentSettingsPattern allowed_pattern =
        ContentSettingsPattern::FromString(pattern);

    ios::HostContentSettingsMapFactory::GetForBrowserState(
        chrome_browser_state_.get())
        ->SetContentSettingCustomScope(
            allowed_pattern, ContentSettingsPattern::Wildcard(),
            ContentSettingsType::POPUPS, std::string(), CONTENT_SETTING_ALLOW);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              ios::HostContentSettingsMapFactory::GetForBrowserState(
                  chrome_browser_state_.get())
                  ->GetContentSetting(url, url, ContentSettingsType::POPUPS,
                                      std::string()));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  UINavigationController* navigation_controller_;
};

TEST_F(BlockPopupsTableViewControllerTest, TestPopupsNotAllowed) {
  SetDisallowPopups();
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());
}

TEST_F(BlockPopupsTableViewControllerTest, TestPopupsAllowed) {
  SetAllowPopups();
  CreateController();
  CheckController();
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_FALSE([controller() navigationItem].rightBarButtonItem);
}

TEST_F(BlockPopupsTableViewControllerTest, TestPopupsAllowedWithOneItem) {
  // Ensure that even if there are 'allowed' patterns, if block popups is
  // turned off (popups are allowed), there is no list of patterns.
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));
  SetAllowPopups();

  CreateController();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_FALSE([controller() navigationItem].rightBarButtonItem);
}

TEST_F(BlockPopupsTableViewControllerTest, TestOneAllowedItem) {
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));

  CreateController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  CheckSectionHeaderWithId(IDS_IOS_POPUPS_ALLOWED, 1);
  CheckTextCellText(base::SysUTF8ToNSString(kAllowedPattern), 1, 0);
  EXPECT_TRUE([controller() navigationItem].rightBarButtonItem);
}

TEST_F(BlockPopupsTableViewControllerTest, TestOneAllowedItemDeleted) {
  // Get the number of entries before testing, to ensure after adding and
  // deleting, the entries are the same.
  ContentSettingsForOneType initial_entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_browser_state_.get())
      ->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                              &initial_entries);

  // Add the pattern to be deleted.
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));

  // Make sure adding the pattern changed the settings size.
  ContentSettingsForOneType added_entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_browser_state_.get())
      ->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                              &added_entries);
  EXPECT_NE(initial_entries.size(), added_entries.size());

  CreateController();

  BlockPopupsTableViewController* popups_controller =
      static_cast<BlockPopupsTableViewController*>(controller());
  // Put the collectionView in 'edit' mode.
  [popups_controller editButtonPressed];
  [popups_controller
      deleteItems:@[ [NSIndexPath indexPathForRow:0 inSection:1] ]];

  // Verify the resulting UI.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return NumberOfSections() == 1 && NumberOfItemsInSection(0) == 1;
      }));

  // Verify that there are no longer any allowed patterns in |profile_|.
  ContentSettingsForOneType final_entries;
  ios::HostContentSettingsMapFactory::GetForBrowserState(
      chrome_browser_state_.get())
      ->GetSettingsForOneType(ContentSettingsType::POPUPS, std::string(),
                              &final_entries);
  EXPECT_EQ(initial_entries.size(), final_entries.size());
}

}  // namespace

// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/block_popups_table_view_controller.h"

#import <Foundation/Foundation.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "url/gurl.h"

namespace {

const char* kAllowedPattern = "[*.]example.com";
const char* kAllowedURL = "http://example.com";
const char* kAllowedPattern2 = "[*.]example.net";
const char* kAllowedURL2 = "http://example.net";
const char* kAllowedPattern3 = "[*.]example.org";
const char* kAllowedURL3 = "http://example.org";

class BlockPopupsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    TestProfileIOS::Builder builder;
    profile_ = std::move(builder).Build();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return
        [[BlockPopupsTableViewController alloc] initWithProfile:profile_.get()];
  }

  void SetDisallowPopups() {
    ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
        ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_BLOCK);
  }

  void SetAllowPopups() {
    ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
        ->SetDefaultContentSetting(ContentSettingsType::POPUPS,
                                   CONTENT_SETTING_ALLOW);
  }

  void AddAllowedPattern(const std::string& pattern, const GURL& url) {
    ContentSettingsPattern allowed_pattern =
        ContentSettingsPattern::FromString(pattern);

    ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
        ->SetContentSettingCustomScope(
            allowed_pattern, ContentSettingsPattern::Wildcard(),
            ContentSettingsType::POPUPS, CONTENT_SETTING_ALLOW);
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                  ->GetContentSetting(url, url, ContentSettingsType::POPUPS));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
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
  CheckTextCellTextAndDetailText(base::SysUTF8ToNSString(kAllowedPattern), nil,
                                 1, 0);
  EXPECT_TRUE([controller() navigationItem].rightBarButtonItem);
}

// Tests deleting one entry for the TableView.
TEST_F(BlockPopupsTableViewControllerTest, TestOneAllowedItemDeleted) {
  // Get the number of entries before testing, to ensure after adding and
  // deleting, the entries are the same.
  ContentSettingsForOneType initial_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);

  // Add the pattern to be deleted.
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));

  // Make sure adding the pattern changed the settings size.
  ContentSettingsForOneType added_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);
  EXPECT_NE(initial_entries.size(), added_entries.size());

  CreateController();

  BlockPopupsTableViewController* popups_controller =
      static_cast<BlockPopupsTableViewController*>(controller());
  [popups_controller deleteItems:@[ [NSIndexPath indexPathForRow:0
                                                       inSection:1] ]];

  // Verify the resulting UI.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool() {
        return NumberOfSections() == 1 && NumberOfItemsInSection(0) == 1;
      }));

  // Verify that there are no longer any allowed patterns in `profile_`.
  ContentSettingsForOneType final_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);
  EXPECT_EQ(initial_entries.size(), final_entries.size());
}

// Tests deleting 2 items.
TEST_F(BlockPopupsTableViewControllerTest, TestMultipleAllowedItemsDeleted) {
  // Get the number of entries before testing, to ensure after adding and
  // deleting, the entries are the same.
  ContentSettingsForOneType initial_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);

  // Add 3 patterns.
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));
  AddAllowedPattern(kAllowedPattern2, GURL(kAllowedURL2));
  AddAllowedPattern(kAllowedPattern3, GURL(kAllowedURL3));

  std::map<std::string, std::string> patterns_to_url;
  patterns_to_url.insert(
      std::pair<std::string, std::string>(kAllowedPattern, kAllowedURL));
  patterns_to_url.insert(
      std::pair<std::string, std::string>(kAllowedPattern2, kAllowedURL2));
  patterns_to_url.insert(
      std::pair<std::string, std::string>(kAllowedPattern3, kAllowedURL3));

  // Make sure adding the pattern changed the settings size.
  ContentSettingsForOneType added_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);
  EXPECT_NE(initial_entries.size(), added_entries.size());

  CreateController();

  BlockPopupsTableViewController* popups_controller =
      static_cast<BlockPopupsTableViewController*>(controller());

  // Check that 3 items are displayed.
  ASSERT_EQ(3L, [popups_controller.tableViewModel numberOfItemsInSection:1]);

  NSIndexPath* first_index = [NSIndexPath indexPathForRow:0 inSection:1];
  NSIndexPath* second_index = [NSIndexPath indexPathForRow:1 inSection:1];
  TableViewDetailTextItem* first_item =
      base::apple::ObjCCastStrict<TableViewDetailTextItem>(
          [popups_controller.tableViewModel itemAtIndexPath:first_index]);
  TableViewDetailTextItem* second_item =
      base::apple::ObjCCastStrict<TableViewDetailTextItem>(
          [popups_controller.tableViewModel itemAtIndexPath:second_index]);

  std::set<std::string> deleted_patterns{
      base::SysNSStringToUTF8(first_item.text),
      base::SysNSStringToUTF8(second_item.text)};

  // Delete patterns 1 and 2. 3 should be left.
  [popups_controller deleteItems:@[ first_index, second_index ]];

  // Wait for the items to be removed.
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForUIElementTimeout, ^bool {
        return [popups_controller.tableViewModel numberOfItemsInSection:1] == 1;
      }));

  std::vector<std::string> blocked_urls;
  std::vector<std::string> allowed_urls;
  for (const auto& [pattern, url] : patterns_to_url) {
    if (base::Contains(deleted_patterns, pattern)) {
      blocked_urls.push_back(url);
    } else {
      allowed_urls.push_back(url);
    }
  }

  // URL3 should be allowed, 1 and 2 shouldn't be.
  ASSERT_EQ(1UL, allowed_urls.size());
  ASSERT_EQ(2UL, blocked_urls.size());

  for (std::string url : blocked_urls) {
    EXPECT_EQ(CONTENT_SETTING_BLOCK,
              ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                  ->GetContentSetting(GURL(url), GURL(url),
                                      ContentSettingsType::POPUPS));
  }
  for (std::string url : allowed_urls) {
    EXPECT_EQ(CONTENT_SETTING_ALLOW,
              ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                  ->GetContentSetting(GURL(url), GURL(url),
                                      ContentSettingsType::POPUPS));
  }
}

// Tests removing the last 3 URLs. Regression test for https://crbug.com/1232905
TEST_F(BlockPopupsTableViewControllerTest, TestMultipleAllowedItemsDeleted2) {
  // Get the number of entries before testing, to ensure after adding and
  // deleting, the entries are the same.
  ContentSettingsForOneType initial_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);

  // Add 3 patterns.
  AddAllowedPattern(kAllowedPattern, GURL(kAllowedURL));
  AddAllowedPattern(kAllowedPattern2, GURL(kAllowedURL2));
  AddAllowedPattern(kAllowedPattern3, GURL(kAllowedURL3));

  // Make sure adding the pattern changed the settings size.
  ContentSettingsForOneType added_entries =
      ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
          ->GetSettingsForOneType(ContentSettingsType::POPUPS);
  EXPECT_NE(initial_entries.size(), added_entries.size());

  CreateController();

  BlockPopupsTableViewController* popups_controller =
      static_cast<BlockPopupsTableViewController*>(controller());
  [popups_controller deleteItems:@[
    [NSIndexPath indexPathForRow:0 inSection:1],
    [NSIndexPath indexPathForRow:1 inSection:1],
    [NSIndexPath indexPathForRow:2 inSection:1]
  ]];

  // No URL should be allowed.
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                ->GetContentSetting(GURL(kAllowedURL), GURL(kAllowedURL),
                                    ContentSettingsType::POPUPS));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                ->GetContentSetting(GURL(kAllowedURL2), GURL(kAllowedURL2),
                                    ContentSettingsType::POPUPS));
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            ios::HostContentSettingsMapFactory::GetForProfile(profile_.get())
                ->GetContentSetting(GURL(kAllowedURL3), GURL(kAllowedURL3),
                                    ContentSettingsType::POPUPS));
}

}  // namespace

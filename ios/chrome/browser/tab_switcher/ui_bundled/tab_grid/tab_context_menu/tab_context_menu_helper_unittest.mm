// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_context_menu_helper.h"

#import "base/test/scoped_feature_list.h"
#import "components/send_tab_to_self/entry_point_display_reason.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_item.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_state.h"
#import "testing/platform_test.h"

@interface TabContextMenuHelper (Testing)
- (BOOL)canCloseOtherTabsForTabWithID:(web::WebStateID)tabID;
- (BOOL)canSendToYourDevicesForItem:(TabItem*)item;
@end

namespace {

// Creates a BrowserList for the given profile.
std::unique_ptr<KeyedService> BuildBrowserList(ProfileIOS* profile) {
  return std::make_unique<BrowserList>();
}

// Test fixture for TabContextMenuHelper.
class TabContextMenuHelperTest : public PlatformTest {
 public:
  TabContextMenuHelperTest() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(BrowserListFactory::GetInstance(),
                              base::BindRepeating(&BuildBrowserList));
    builder.AddTestingFactory(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](ProfileIOS* profile) -> std::unique_ptr<KeyedService> {
              return std::make_unique<
                  send_tab_to_self::StubSendTabToSelfSyncService>();
            }));
    profile_ = std::move(builder).Build();

    browser_ = std::make_unique<TestBrowser>(profile_.get());

    // Add browser to BrowserList
    BrowserList* list = BrowserListFactory::GetForProfile(profile_.get());
    list->AddBrowser(browser_.get());

    helper_ = [[TabContextMenuHelper alloc] initWithProfile:profile_.get()
                                     tabContextMenuDelegate:nil];
  }

  void SetUp() override { PlatformTest::SetUp(); }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  TabContextMenuHelper* helper_;
};

// Tests "Close Other Tabs" with only regular tabs.
TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_RegularTabs) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a*"));
  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  // Only 1 regular tab, should be disabled.
  EXPECT_FALSE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);

  // Clear logic not needed as we split tests, but if we wanted to reuse:
  // browser_->GetWebStateList()->CloseAllWebStates(...);
}

TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_MultipleRegularTabs) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c"));
  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  web::WebStateID identifier_b =
      builder.GetWebStateForIdentifier('b')->GetUniqueIdentifier();

  // Multiple regular tabs.
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_b]);
}

TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_PinnedTabOnly) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a |"));
  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  // 1 pinned tab, 0 regular tabs.
  EXPECT_FALSE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
}

TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_PinnedAndRegularTabs) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b*"));
  EXPECT_EQ(2, browser_->GetWebStateList()->count());
  EXPECT_EQ(1, browser_->GetWebStateList()->pinned_tabs_count());

  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  web::WebStateID identifier_b =
      builder.GetWebStateForIdentifier('b')->GetUniqueIdentifier();

  // 'a' is pinned. Should be able to close 'b' (regular).
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
  // 'b' is regular. Should NOT be able to close others (no other regular tabs).
  EXPECT_FALSE([helper_ canCloseOtherTabsForTabWithID:identifier_b]);
}

TEST_F(TabContextMenuHelperTest,
       CanCloseOtherTabs_PinnedAndMultipleRegularTabs) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("a | b* c"));
  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  web::WebStateID identifier_b =
      builder.GetWebStateForIdentifier('b')->GetUniqueIdentifier();
  web::WebStateID identifier_c =
      builder.GetWebStateForIdentifier('c')->GetUniqueIdentifier();

  // 'a' is pinned. Should be able to close 'b' and 'c'.
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
  // 'b' is regular. Should be able to close 'c'.
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_b]);
  // 'c' is regular. Should be able to close 'b'.
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_c]);
}

// Tests "Close Other Tabs" for a single tab in a group.
TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_SingleTabInGroup) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  // Create a group with identifier '0' containing regular tab 'a'.
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a* ]"));
  EXPECT_EQ(1, browser_->GetWebStateList()->count());

  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();

  // Only 1 tab in the group, should be disabled.
  EXPECT_FALSE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
}

// Tests "Close Other Tabs" for multiple tabs in a group.
TEST_F(TabContextMenuHelperTest, CanCloseOtherTabs_MultipleTabsInGroup) {
  WebStateListBuilderFromDescription builder(browser_->GetWebStateList());
  // Create a group with identifier '0' containing regular tabs 'a' and 'b'.
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 0 a* b ]"));
  EXPECT_EQ(2, browser_->GetWebStateList()->count());

  web::WebStateID identifier_a =
      builder.GetWebStateForIdentifier('a')->GetUniqueIdentifier();
  web::WebStateID identifier_b =
      builder.GetWebStateForIdentifier('b')->GetUniqueIdentifier();

  // Multiple tabs in the group, should be enabled.
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_a]);
  EXPECT_TRUE([helper_ canCloseOtherTabsForTabWithID:identifier_b]);
}

// Tests that `canSendToYourDevicesForItem:` returns NO if the feature flag is
// disabled.
TEST_F(TabContextMenuHelperTest, CanSendToYourDevices_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      send_tab_to_self::kSendTabToSelfExtraEntryPoints);

  TabItem* item = [[TabItem alloc] initWithTitle:@"Google"
                                             URL:GURL("https://google.com")];
  EXPECT_FALSE([helper_ canSendToYourDevicesForItem:item]);
}

// Tests that `canSendToYourDevicesForItem:` returns YES if the feature flag is
// enabled and service offers it.
TEST_F(TabContextMenuHelperTest, CanSendToYourDevices_FeatureEnabled) {
  base::test::ScopedFeatureList scoped_feature_list{
      send_tab_to_self::kSendTabToSelfExtraEntryPoints};

  send_tab_to_self::StubSendTabToSelfSyncService* service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  service->SetEntryPointDisplayReason(
      send_tab_to_self::EntryPointDisplayReason::kOfferFeature);

  TabItem* item = [[TabItem alloc] initWithTitle:@"Google"
                                             URL:GURL("https://google.com")];
  EXPECT_TRUE([helper_ canSendToYourDevicesForItem:item]);
}

// Tests that `canSendToYourDevicesForItem:` returns NO if the sync service does
// not offer it.
TEST_F(TabContextMenuHelperTest, CanSendToYourDevices_NoDisplayReason) {
  base::test::ScopedFeatureList scoped_feature_list{
      send_tab_to_self::kSendTabToSelfExtraEntryPoints};

  send_tab_to_self::StubSendTabToSelfSyncService* service =
      static_cast<send_tab_to_self::StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(profile_.get()));
  service->SetEntryPointDisplayReason(std::nullopt);

  TabItem* item = [[TabItem alloc] initWithTitle:@"Google"
                                             URL:GURL("https://google.com")];
  EXPECT_FALSE([helper_ canSendToYourDevicesForItem:item]);
}

// Tests that `canSendToYourDevicesForItem:` returns NO if the profile is
// incognito.
TEST_F(TabContextMenuHelperTest, CanSendToYourDevices_Incognito) {
  base::test::ScopedFeatureList scoped_feature_list{
      send_tab_to_self::kSendTabToSelfExtraEntryPoints};

  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  TabContextMenuHelper* otr_helper =
      [[TabContextMenuHelper alloc] initWithProfile:otr_profile
                             tabContextMenuDelegate:nil];

  TabItem* item = [[TabItem alloc] initWithTitle:@"Google"
                                             URL:GURL("https://google.com")];
  EXPECT_FALSE([otr_helper canSendToYourDevicesForItem:item]);
}

}  // namespace

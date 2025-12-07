// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/regular/regular_grid_mediator.h"

#import "base/containers/contains.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/collaboration/test_support/mock_messaging_backend_service.h"
#import "components/data_sharing/public/features.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/saved_tab_groups/ui/fake_face_pile_provider.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/regular/regular_grid_mediator_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/fake_tab_collection_consumer.h"
#import "ios/web/public/test/fakes/fake_web_state.h"

using collaboration::messaging::PersistentNotificationType;
using testing::_;
using testing::Return;

@interface FakeRegularGridMediatorDelegate
    : NSObject <RegularGridMediatorDelegate>
@end

@implementation FakeRegularGridMediatorDelegate

- (id<FacePileProviding>)facePileProviderForGroupID:(const std::string&)groupID
                                         groupColor:(UIColor*)groupColor {
  return [[FakeFacePileProvider alloc] init];
}

@end

@interface TestRegularGridMediator
    : RegularGridMediator <MessagingBackendServiceObserving,
                           TabGroupSyncServiceObserverDelegate>
@end

@implementation TestRegularGridMediator
@end

class RegularGridMediatorTest : public GridMediatorTestClass {
 public:
  RegularGridMediatorTest() {}
  ~RegularGridMediatorTest() override {}

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            data_sharing::features::kDataSharingFeature,
        },
        /*disable_features=*/{});

    GridMediatorTestClass::SetUp();
    mode_holder_ = [[TabGridModeHolder alloc] init];
    share_kit_service_ = std::make_unique<TestShareKitService>(
        nullptr, nullptr, nullptr, tab_group_service_);

    mediator_ = [[TestRegularGridMediator alloc]
         initWithModeHolder:mode_holder_
        tabGroupSyncService:tab_group_sync_service_.get()
            shareKitService:share_kit_service_.get()
           messagingService:&messaging_backend_];
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();
    mediator_.toolbarsMutator = fake_toolbars_mediator_;
    [mediator_ currentlySelectedGrid:YES];

    tab_restore_service_ =
        IOSChromeTabRestoreServiceFactory::GetForProfile(profile_.get());
  }

  void TearDown() override {
    [mediator_ disconnect];
    GridMediatorTestClass::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestRegularGridMediator* mediator_ = nullptr;
  std::unique_ptr<ShareKitService> share_kit_service_;
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;
  TabGridModeHolder* mode_holder_;
  collaboration::messaging::MockMessagingBackendService messaging_backend_;
};

#pragma mark - Command tests

// Tests that the WebStateList and consumer's list are empty when
// `-saveAndCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, SaveAndCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
}

// Tests that the WebStateList is not restored to 3 items when
// `-undoCloseAllItems` is called after `-discardSavedClosedItems` is called.
TEST_F(RegularGridMediatorTest, DiscardSavedClosedItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ discardSavedClosedItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, UndoCloseAllItemsCommand) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(3UL, consumer_.items.size());
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[0]));
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[1]));
  EXPECT_TRUE(base::Contains(original_identifiers_, consumer_.items[2]));
}

// Tests that the WebStateList is restored to 3 items when
// `-undoCloseAllItems` is called.
TEST_F(RegularGridMediatorTest, UndoCloseAllItemsCommandWithNTP) {
  // Previously there were 3 items.
  [mediator_ saveAndCloseAllItems];

  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());

  // There should be no "recently closed items" yet.
  EXPECT_EQ(0u, tab_restore_service_->entries().size());

  // Discarding the saved item should add them to recently closed.
  [mediator_ discardSavedClosedItems];
  EXPECT_EQ(3u, tab_restore_service_->entries().size());

  // Add three new tabs.
  auto web_state1 = CreateFakeWebStateWithURL(GURL("https://test/url1"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state1), WebStateList::InsertionParams::AtIndex(0));
  // Second tab is a NTP.
  auto web_state2 = CreateFakeWebStateWithURL(GURL(kChromeUINewTabURL));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state2), WebStateList::InsertionParams::AtIndex(1));
  auto web_state3 = CreateFakeWebStateWithURL(GURL("https://test/url2"));
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state3), WebStateList::InsertionParams::AtIndex(2));
  browser_->GetWebStateList()->ActivateWebStateAt(0);

  // Closing item does not add them to the recently closed.
  [mediator_ saveAndCloseAllItems];

  // There should be no tabs in the WebStateList.
  EXPECT_EQ(0, browser_->GetWebStateList()->count());
  EXPECT_EQ(0UL, consumer_.items.size());

  // There should be no new "recently closed items".
  EXPECT_EQ(3u, tab_restore_service_->entries().size());

  // Undoing the close should restore the items.
  [mediator_ undoCloseAllItems];
  EXPECT_EQ(3UL, consumer_.items.size());
}

// Checks that opening a new regular tab from the toolbar is done when allowed.
TEST_F(RegularGridMediatorTest, OpenNewTab_OpenIfAllowedByPolicy) {
  // IncognitoModePrefs::kEnabled Means that users may open pages in both
  // Incognito mode and normal mode
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kEnabled)));
  EXPECT_EQ(3, browser_->GetWebStateList()->count());

  // Emulate tapping one the new tab button by using the actions wrangler
  // interface that would normally be called by the tap action target.
  [mediator_ newTabButtonTapped:nil];

  EXPECT_EQ(4, browser_->GetWebStateList()->count())
      << "Can not open a regular tab by calling new tab button function when "
         "policy is the default value.";

  // IncognitoModePrefs::kDisabled Means that users may not open pages in
  // Incognito mode. Only normal mode is available for browsing.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kDisabled)));

  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(5, browser_->GetWebStateList()->count())
      << "Can not open a regular tab by calling new tab button function when "
         "policy should disable incognito.";

  // IncognitoModePrefs::kForced Means that users may open pages *ONLY* in
  // Incognito mode. Normal mode is not available for browsing.
  profile_->GetTestingPrefService()->SetManagedPref(
      policy::policy_prefs::kIncognitoModeAvailability,
      std::make_unique<base::Value>(
          static_cast<int>(IncognitoModePrefs::kForced)));
  EXPECT_EQ(5, browser_->GetWebStateList()->count());
  [mediator_ newTabButtonTapped:nil];
  EXPECT_EQ(5, browser_->GetWebStateList()->count())
      << "Can open a regular tab by calling new tab button function when "
         "policy force incognito only.";
}

// Ensures that when there is *no* web states in normal mode, the toolbar
// configuration is correct.
TEST_F(RegularGridMediatorTest, TestToolbarsNormalModeWithoutWebstates) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ saveAndCloseAllItems];
  EXPECT_EQ(0UL, consumer_.items.size());

  EXPECT_EQ(TabGridPageRegularTabs, fake_toolbars_mediator_.configuration.page);

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.searchButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.undoButton);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.addToButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}

// Tests that `facePileProviderForItem` returns an UIView when the group is
// shared.
TEST_F(RegularGridMediatorTest, facePileProviderForItem) {
  FakeRegularGridMediatorDelegate* fakeDelegate =
      [[FakeRegularGridMediatorDelegate alloc] init];
  mediator_.regularDelegate = fakeDelegate;

  // Set a saved tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  const TabGroup* local_group = browser_->GetWebStateList()->CreateGroup(
      {2}, tab_groups::test::CreateTabGroupVisualData(), tab_group_id);
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group.saved_guid()).has_value());

  GridItemIdentifier* group_item_id =
      [GridItemIdentifier groupIdentifier:local_group];
  EXPECT_EQ(nil, [mediator_ facePileProviderForItem:group_item_id]);

  // Share the group.
  tab_group_sync_service_->MakeTabGroupShared(
      group.local_group_id().value(), syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());
  EXPECT_NE(nil, [mediator_ facePileProviderForItem:group_item_id]);
}

// Tests that `-activityLabelDataForGroup:` returns the data for a specific tab
// group after the messaging backend service is initialized.
TEST_F(RegularGridMediatorTest, ActivityLabelDataForGroupAfterStartup) {
  // Create a saved tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  browser_->GetWebStateList()->CreateGroup(
      {2}, tab_groups::test::CreateTabGroupVisualData(), tab_group_id);
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group.saved_guid()).has_value());

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabGroupMessageMetadata metadata;
  metadata.local_tab_group_id = tab_group_id;
  message.attribution.tab_metadata =
      std::make_optional(collaboration::messaging::TabMessageMetadata());
  message.attribution.tab_group_metadata = std::make_optional(metadata);
  metadata.local_tab_group_id = std::make_optional(tab_group_id);
  message.type = PersistentNotificationType::DIRTY_TAB;
  message.collaboration_event =
      collaboration::messaging::CollaborationEvent::TAB_UPDATED;

  // The activity label data should be nil before the messaging service backend
  // is initialized.
  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(false));
  EXPECT_EQ(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));
  ON_CALL(messaging_backend_,
          GetMessagesForGroup(tab_groups::EitherGroupID(tab_group_id),
                              PersistentNotificationType::DIRTY_TAB))
      .WillByDefault(Return(std::vector{message}));

  // Fake the initialization of the service.
  [mediator_ onMessagingBackendServiceInitialized];

  // The activity label data should exist after the messaging service backend is
  // initialized.
  EXPECT_NE(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);

  // The activity label data should be nil for another group.
  EXPECT_EQ(
      nil,
      [mediator_
          activityLabelDataForGroup:tab_groups::TabGroupId::GenerateNew()]);

  // Simulate the tab message being removed.
  ON_CALL(messaging_backend_,
          GetMessagesForGroup(tab_groups::EitherGroupID(tab_group_id),
                              PersistentNotificationType::DIRTY_TAB))
      .WillByDefault(
          Return(std::vector<collaboration::messaging::PersistentMessage>{}));
  // Fake the update of the service.
  [mediator_ hidePersistentMessage:message];

  // The activity label data should be nil.
  EXPECT_EQ(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);
}

// Tests that `-activityLabelDataForGroup:` returns the data for a specific tab
// group after the API to display the UI is called.
TEST_F(RegularGridMediatorTest,
       ActivityLabelDataForGroupAfterDisplayAPICalled) {
  // Create a saved tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  browser_->GetWebStateList()->CreateGroup(
      {2}, tab_groups::test::CreateTabGroupVisualData(), tab_group_id);
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group.saved_guid()).has_value());

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabGroupMessageMetadata metadata;
  metadata.local_tab_group_id = std::make_optional(tab_group_id);
  message.type = PersistentNotificationType::DIRTY_TAB_GROUP;
  message.attribution.tab_group_metadata = std::make_optional(metadata);

  // The activity label data should be nil by default.
  EXPECT_EQ(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));

  // Fake the update of the service.
  [mediator_ displayPersistentMessage:message];

  // The activity label data should exist after the messaging service backend is
  // initialized.
  EXPECT_NE(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);

  // The activity label data should be nil for another group.
  EXPECT_EQ(
      nil,
      [mediator_
          activityLabelDataForGroup:tab_groups::TabGroupId::GenerateNew()]);

  // Fake the update of the service.
  [mediator_ hidePersistentMessage:message];

  // The activity label data should be nil.
  EXPECT_EQ(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);
}

// Tests that `-activityLabelDataForGroup:` returns the data for a specific tab
// group after simulating a tab removed.
TEST_F(RegularGridMediatorTest, ActivityLabelDataForGroupAfterTabRemoved) {
  // Create a saved tab group.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  browser_->GetWebStateList()->CreateGroup(
      {2}, tab_groups::test::CreateTabGroupVisualData(), tab_group_id);
  tab_groups::SavedTabGroup group = tab_groups::test::CreateTestSavedTabGroup();
  group.SetLocalGroupId(tab_group_id);
  tab_group_sync_service_->AddGroup(group);
  EXPECT_TRUE(
      tab_group_sync_service_->GetGroup(group.saved_guid()).has_value());

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabGroupMessageMetadata metadata;
  metadata.local_tab_group_id = tab_group_id;
  message.attribution.tab_metadata =
      std::make_optional(collaboration::messaging::TabMessageMetadata());
  message.attribution.tab_group_metadata = std::make_optional(metadata);
  metadata.local_tab_group_id = std::make_optional(tab_group_id);
  message.type = PersistentNotificationType::TOMBSTONED;
  message.collaboration_event =
      collaboration::messaging::CollaborationEvent::TAB_REMOVED;

  // The activity label data should be nil for another group.
  EXPECT_EQ(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);

  ON_CALL(messaging_backend_,
          GetMessagesForGroup(tab_groups::EitherGroupID(tab_group_id),
                              PersistentNotificationType::TOMBSTONED))
      .WillByDefault(Return(std::vector{message}));

  // Fake the update of the service.
  [mediator_ hidePersistentMessage:message];

  // The activity label data should be nil.
  EXPECT_NE(nil, [mediator_ activityLabelDataForGroup:tab_group_id]);
}

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/collaboration/test_support/mock_collaboration_service.h"
#import "components/collaboration/test_support/mock_messaging_backend_service.h"
#import "components/data_sharing/public/features.h"
#import "components/data_sharing/test_support/mock_data_sharing_service.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#import "ios/chrome/browser/collaboration/model/collaboration_service_factory.h"
#import "ios/chrome/browser/collaboration/model/messaging/messaging_backend_service_bridge.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_service.h"
#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_sync_service_observer_bridge.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/test/fake_tab_collection_consumer.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

using tab_groups::SavedTabGroup;
using tab_groups::SavedTabGroupTab;
using testing::_;
using testing::Return;

@interface TestTabGroupMediator
    : TabGroupMediator <MessagingBackendServiceObserving,
                        TabGroupSyncServiceObserverDelegate>
@end

@implementation TestTabGroupMediator
@end

namespace {

// Creates a vector of `saved_tabs` based on the given `range`.
std::vector<SavedTabGroupTab> SavedTabGroupTabsFromTabs(
    std::vector<int> indexes,
    WebStateList* web_state_list,
    base::Uuid saved_tab_group_id) {
  std::vector<SavedTabGroupTab> saved_tabs;
  for (int index : indexes) {
    web::WebState* web_state = web_state_list->GetWebStateAt(index);
    SavedTabGroupTab saved_tab(web_state->GetVisibleURL(),
                               web_state->GetTitle(), saved_tab_group_id,
                               std::make_optional(index), std::nullopt,
                               web_state->GetUniqueIdentifier().identifier());
    saved_tabs.push_back(saved_tab);
  }
  return saved_tabs;
}

}  // namespace

class TabGroupMediatorTest : public GridMediatorTestClass {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {data_sharing::features::kDataSharingFeature}, {});

    GridMediatorTestClass::SetUp();

    WebStateList* web_state_list = browser_->GetWebStateList();
    CloseAllWebStates(*web_state_list, WebStateList::ClosingReason::kDefault);
    builder_ =
        std::make_unique<WebStateListBuilderFromDescription>(web_state_list);
    ASSERT_TRUE(builder_->BuildWebStateListFromDescription(
        "| f [ 1 a* b c ] d e ", browser_->GetProfile()));

    mode_holder_ = [[TabGridModeHolder alloc] init];
    tab_group_ = web_state_list->GetGroupOfWebStateAt(1);
    tab_group_consumer_ = OCMProtocolMock(@protocol(TabGroupConsumer));
    share_kit_service_ = std::make_unique<TestShareKitService>(
        nullptr, nullptr, nullptr, tab_group_service_);
    collaboration_service_ =
        std::make_unique<collaboration::MockCollaborationService>();
    data_sharing_service_ = std::make_unique<
        ::testing::NiceMock<data_sharing::MockDataSharingService>>();

    base::Uuid saved_tab_group_id = base::Uuid::GenerateRandomV4();
    std::vector<SavedTabGroupTab> saved_tabs = SavedTabGroupTabsFromTabs(
        {1, 2, 3}, web_state_list, saved_tab_group_id);
    SavedTabGroup saved_group(
        base::SysNSStringToUTF16(tab_group_->GetRawTitle()),
        tab_group_->visual_data().color(), saved_tabs, std::nullopt,
        saved_tab_group_id, tab_group_->tab_group_id());
    tab_group_sync_service_->AddGroup(saved_group);

    EXPECT_CALL(*collaboration_service_, GetServiceStatus()).Times(1);

    mediator_ = [[TestTabGroupMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()
         tabGroupSyncService:tab_group_sync_service_.get()
             shareKitService:share_kit_service_.get()
        collaborationService:collaboration_service_.get()
          dataSharingService:data_sharing_service_.get()
                    tabGroup:tab_group_->GetWeakPtr()
                    consumer:tab_group_consumer_
                gridConsumer:consumer_
                  modeHolder:mode_holder_
            messagingService:&messaging_backend_
            tabGroupDelegate:nil];
    mediator_.browser = browser_.get();
  }

  void TearDown() override {
    [mediator_ disconnect];
    GridMediatorTestClass::TearDown();
  }

  // Checks that the drag item origin metric is logged in UMA.
  void ExpectThatDragItemOriginMetricLogged(DragItemOrigin origin,
                                            int count = 1) {
    histogram_tester_.ExpectUniqueSample(kUmaGroupViewDragOrigin, origin,
                                         count);
  }

 protected:
  TestTabGroupMediator* mediator_;
  id<TabGroupConsumer> tab_group_consumer_;
  raw_ptr<const TabGroup, DanglingUntriaged> tab_group_;
  std::unique_ptr<WebStateListBuilderFromDescription> builder_;
  std::unique_ptr<ShareKitService> share_kit_service_;
  std::unique_ptr<collaboration::MockCollaborationService>
      collaboration_service_;
  std::unique_ptr<data_sharing::MockDataSharingService> data_sharing_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  TabGridModeHolder* mode_holder_;
  collaboration::messaging::MockMessagingBackendService messaging_backend_;
};

// Tests dropping a local tab (e.g. drag from same window) in the grid.
TEST_F(TabGroupMediatorTest, DropLocalTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  EXPECT_EQ("| f [ 1 a* d b c ] e", builder_->GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests dropping tabs from the grid to a tab group.
TEST_F(TabGroupMediatorTest, DropFromTabGrid) {
  WebStateList* web_state_list = browser_->GetWebStateList();

  // Drop "F" before "A".
  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(0)->GetUniqueIdentifier();
  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:0 fromSameCollection:NO];
  EXPECT_EQ("| [ 1 f a* b c ] d e", builder_->GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 1);

  // Drop "D" before "B".
  web_state_id = web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:2 fromSameCollection:NO];
  EXPECT_EQ("| [ 1 f a* d b c ] e", builder_->GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 2);
}

// Tests dropping a tab from another browser (e.g. drag from another window) in
// the grid.
TEST_F(TabGroupMediatorTest, DropCrossWindowTab) {
  auto other_browser = std::make_unique<TestBrowser>(
      profile_.get(), scene_state_,
      std::make_unique<BrowserWebStateListDelegate>(profile_.get()));
  SnapshotBrowserAgent::CreateForBrowser(other_browser.get());

  browser_list_->AddBrowser(other_browser.get());

  GURL url_to_load = GURL("https://dragged_url.com");
  std::unique_ptr<web::FakeWebState> other_web_state =
      CreateFakeWebStateWithURL(url_to_load);
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  other_browser->GetWebStateList()->InsertWebState(std::move(other_web_state));

  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());

  id local_object = [[TabInfo alloc] initWithTabID:other_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:3 fromSameCollection:NO];

  EXPECT_EQ(7, web_state_list->count());
  EXPECT_EQ(0, other_browser->GetWebStateList()->count());
  EXPECT_EQ(url_to_load, web_state_list->GetWebStateAt(4)->GetVisibleURL());
  EXPECT_EQ(tab_group_, web_state_list->GetGroupOfWebStateAt(4));
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOtherBrowser);
}

// Tests dropping an interal URL (e.g. drag from omnibox) in the grid.
TEST_F(TabGroupMediatorTest, DropInternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());

  GURL url_to_load = GURL("https://dragged_url.com");
  id local_object = [[URLInfo alloc] initWithURL:url_to_load title:@"My title"];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:NO];

  EXPECT_EQ(7, web_state_list->count());
  web::WebState* web_state = web_state_list->GetWebStateAt(2);
  EXPECT_EQ(url_to_load,
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  EXPECT_EQ(tab_group_, web_state_list->GetGroupOfWebStateAt(2));
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

// Tests dropping an external URL in the grid.
TEST_F(TabGroupMediatorTest, DropExternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());

  NSItemProvider* item_provider = [[NSItemProvider alloc]
      initWithContentsOfURL:[NSURL URLWithString:@"https://dragged_url.com"]];

  // Drop item.
  [mediator_ dropItemFromProvider:item_provider
                          toIndex:0
               placeholderContext:nil];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), ^bool(void) {
        return web_state_list->count() == 7;
      }));
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  EXPECT_EQ(GURL("https://dragged_url.com"),
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  EXPECT_EQ(tab_group_, web_state_list->GetGroupOfWebStateAt(2));
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

// Tests that deleting a group works.
TEST_F(TabGroupMediatorTest, DeleteGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());
  EXPECT_EQ(1u, web_state_list->GetGroups().size());

  [mediator_ deleteGroup];
  ASSERT_EQ(3, web_state_list->count());
  EXPECT_EQ(0u, web_state_list->GetGroups().size());
}

// Tests that ungrouping a group works.
TEST_F(TabGroupMediatorTest, Ungroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());
  EXPECT_EQ(1u, web_state_list->GetGroups().size());

  [mediator_ ungroup];
  ASSERT_EQ(6, web_state_list->count());
  EXPECT_EQ(0u, web_state_list->GetGroups().size());
}

// Tests that closing tabs in a group that is not captured by the current
// mediator removes the group.
TEST_F(TabGroupMediatorTest, CreateAnotherGroupAndCloseTabs) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  ASSERT_EQ(6, web_state_list->count());
  EXPECT_EQ(1u, web_state_list->GetGroups().size());

  tab_groups::TabGroupVisualData visual_data = tab_groups::TabGroupVisualData(
      u"group 2", tab_groups::TabGroupColorId::kRed);
  web_state_list->CreateGroup({4, 5}, visual_data,
                              tab_groups::TabGroupId::GenerateNew());
  EXPECT_EQ("| f [ 1 a* b c ] [ _ d e ]",
            builder_->GetWebStateListDescription());

  web_state_list->CloseWebStateAt(4, WebStateList::ClosingReason::kUserAction);
  web_state_list->CloseWebStateAt(4, WebStateList::ClosingReason::kUserAction);
  EXPECT_EQ("| f [ 1 a* b c ]", builder_->GetWebStateListDescription());
}

// Tests that CollaborationIDChangedForGroup does not update facePile UI when
// the group id does not match.
TEST_F(TabGroupMediatorTest, CollaborationIDChangedForInvalidGroup) {
  OCMReject([tab_group_consumer_ setFacePileProvider:OCMOCK_ANY]);

  SavedTabGroup other_saved_group(
      u"other group", tab_groups::TabGroupColorId::kOrange, {},
      /*position=*/std::nullopt, base::Uuid::GenerateRandomV4(),
      tab_groups::TabGroupId::GenerateNew());
  tab_group_sync_service_->AddGroup(other_saved_group);
  tab_group_sync_service_->MakeTabGroupShared(
      other_saved_group.local_group_id().value(),
      syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());

  EXPECT_OCMOCK_VERIFY((id)tab_group_consumer_);
}

// Tests that CollaborationIDChangedForGroup correctly updates the facePile UI
// when the group is shared.
TEST_F(TabGroupMediatorTest, CollaborationIDChangedForGroupShared) {
  OCMExpect([tab_group_consumer_ setFacePileProvider:OCMOCK_ANY]);

  const SavedTabGroup saved_group =
      tab_group_sync_service_->GetGroup(tab_group_->tab_group_id()).value();
  tab_group_sync_service_->MakeTabGroupShared(
      saved_group.local_group_id().value(),
      syncer::CollaborationId("collaboration"),
      tab_groups::TabGroupSyncService::TabGroupSharingCallback());

  EXPECT_OCMOCK_VERIFY((id)tab_group_consumer_);
}

// Tests that the text in the activity summary is updated when the messaging
// backend service is initialized.
TEST_F(TabGroupMediatorTest, UpdateActivitySummaryTextAfterStartup) {
  OCMExpect([tab_group_consumer_ setActivitySummaryCellText:OCMOCK_ANY]);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebState* web_state = web_state_list->GetWebStateAt(0);

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabMessageMetadata metadata;
  metadata.local_tab_id =
      std::make_optional(web_state->GetUniqueIdentifier().identifier());
  message.type =
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB;
  message.attribution.tab_metadata = std::make_optional(metadata);

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));
  ON_CALL(messaging_backend_, GetMessages(_))
      .WillByDefault(Return(std::vector{message}));

  // Fake the initialization of the service.
  [mediator_ onMessagingBackendServiceInitialized];

  // Expect that `-setActivitySummaryCellText:` is called to update the text in
  // the activity summary.
  EXPECT_OCMOCK_VERIFY((id)tab_group_consumer_);
}

// Tests that the text in the activity summary is updated when the API to
// disaply the UI is called.
TEST_F(TabGroupMediatorTest, UpdateActivitySummaryTextAfterDisplayAPICalled) {
  OCMExpect([tab_group_consumer_ setActivitySummaryCellText:OCMOCK_ANY]);

  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebState* web_state = web_state_list->GetWebStateAt(0);

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabMessageMetadata metadata;
  metadata.local_tab_id =
      std::make_optional(web_state->GetUniqueIdentifier().identifier());
  message.type =
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB;
  message.attribution.tab_metadata = std::make_optional(metadata);

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));

  // Fake the update of the service.
  [mediator_ displayPersistentMessage:message];

  // Expect that `-setActivitySummaryCellText:` is called to update the text in
  // the activity summary.
  EXPECT_OCMOCK_VERIFY((id)tab_group_consumer_);
}

// Tests that the text in the activity summary is NOT updated when the ID in the
// message doesn't match with any displayed items.
TEST_F(TabGroupMediatorTest, DoNotUpdateActivitySummaryTextWithUnmatchedID) {
  OCMExpect([tab_group_consumer_ setActivitySummaryCellText:nil]);

  // Create a fake message.
  collaboration::messaging::PersistentMessage message;
  collaboration::messaging::TabMessageMetadata metadata;
  // Set a new unique ID so that it shouldn't match with any items.
  metadata.local_tab_id =
      std::make_optional(web::WebStateID::NewUnique().identifier());
  message.type =
      collaboration::messaging::PersistentNotificationType::DIRTY_TAB;
  message.attribution.tab_metadata = std::make_optional(metadata);

  ON_CALL(messaging_backend_, IsInitialized).WillByDefault(Return(true));

  // Fake the update of the service.
  [mediator_ displayPersistentMessage:message];

  // Expect that `-setActivitySummaryCellText:` is called with `nil` because the
  // ID in the message doesn't match with any items in the web state list.
  EXPECT_OCMOCK_VERIFY((id)tab_group_consumer_);
}

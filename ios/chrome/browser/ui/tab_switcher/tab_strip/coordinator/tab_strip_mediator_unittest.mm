// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/core/favicon_url.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/public/saved_tab_group_tab.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_last_tab_dragged_alert_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/fake_tab_strip_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/fake_tab_strip_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using tab_groups::SavedTabGroup;
using tab_groups::SavedTabGroupTab;
using tab_groups::TabGroupId;
using testing::_;
using testing::Return;

namespace {

// Fake WebStateList delegate that attaches the required tab helper.
class TabStripFakeWebStateListDelegate : public FakeWebStateListDelegate {
 public:
  TabStripFakeWebStateListDelegate() {}
  ~TabStripFakeWebStateListDelegate() override {}

  // WebStateListDelegate implementation.
  void WillAddWebState(web::WebState* web_state) override {
    SnapshotTabHelper::CreateForWebState(web_state);
  }
};

// URL to be used for drag and drop tests.
const char kDraggedUrl[] = "https://dragged_url.com";
// URL for saved tabs.
const char kSavedTabUrl[] = "https://google.com";
// Title for saved tabs.
const char16_t kSavedTabTitle[] = u"Google";

// Returns a saved tab group for test.
SavedTabGroup TestSavedGroup(
    const base::Uuid& saved_id = base::Uuid::GenerateRandomV4()) {
  SavedTabGroup saved_group(u"Test title", tab_groups::TabGroupColorId::kBlue,
                            {}, std::nullopt, saved_id, std::nullopt);
  return saved_group;
}

MATCHER_P2(TabTitleAndURLEq, title, url, "") {
  return arg.title() == title && arg.url() == url;
}

}  // namespace

// Test fixture for the TabStripMediator.
class TabStripMediatorTest : public PlatformTest {
 public:
  TabStripMediatorTest() {
    feature_list_.InitWithFeatures(
        {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});
    TestProfileIOS::Builder profile_builder;
    profile_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    profile_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    tab_group_sync_service_ = std::make_unique<
        ::testing::NiceMock<tab_groups::MockTabGroupSyncService>>();

    profile_ = std::move(profile_builder).Build();
    browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<TabStripFakeWebStateListDelegate>());
    other_browser_ = std::make_unique<TestBrowser>(
        profile_.get(), std::make_unique<TabStripFakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();

    SnapshotBrowserAgent::CreateForBrowser(other_browser_.get());
    SnapshotBrowserAgent::CreateForBrowser(browser_.get());

    tab_strip_handler_ = [[FakeTabStripHandler alloc] init];

    consumer_ = [[FakeTabStripConsumer alloc] init];
  }

  ~TabStripMediatorTest() override { [mediator_ disconnect]; }

  void InitializeMediator() {
    BrowserList* browser_list =
        BrowserListFactory::GetForProfile(profile_.get());
    browser_list->AddBrowser(browser_.get());
    browser_list->AddBrowser(other_browser_.get());

    mediator_ =
        [[TabStripMediator alloc] initWithConsumer:consumer_
                               tabGroupSyncService:tab_group_sync_service_.get()
                                       browserList:browser_list];
    mediator_.profile = profile_.get();
    mediator_.webStateList = web_state_list_;
    mediator_.browser = browser_.get();
    mediator_.tabStripHandler = tab_strip_handler_;
  }

  void AddWebState(bool pinned = false) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(profile_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate().Pinned(pinned));
  }

  // Returns a new SavedTabGroupTab.
  SavedTabGroupTab CreateSavedTab(const base::Uuid& group_guid) {
    return SavedTabGroupTab(GURL(kSavedTabUrl), kSavedTabTitle, group_guid,
                            std::nullopt);
  }

  // Checks that the drag item origin metric is logged in UMA.
  void ExpectThatDragItemOriginMetricLogged(DragItemOrigin origin,
                                            int count = 1) {
    histogram_tester_.ExpectUniqueSample(kUmaTabStripViewDragOrigin, origin,
                                         count);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  FakeTabStripHandler* tab_strip_handler_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestBrowser> other_browser_;
  raw_ptr<WebStateList> web_state_list_;
  TabStripMediator* mediator_;
  FakeTabStripConsumer* consumer_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<tab_groups::MockTabGroupSyncService> tab_group_sync_service_;
};

// Tests that the mediator correctly populates the consumer at startup and after
// an update of the WebStateList.
TEST_F(TabStripMediatorTest, ConsumerPopulated) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(2ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].tabSwitcherItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].tabSwitcherItem.identifier);

  // Check that the webstate is correctly added to the consumer.
  AddWebState();

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(3ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].tabSwitcherItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].tabSwitcherItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.items[2].tabSwitcherItem.identifier);

  // Check that the webstate is correctly removed from the consumer.
  web_state_list_->CloseWebStateAt(web_state_list_->active_index(),
                                   WebStateList::CLOSE_USER_ACTION);

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(2ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].tabSwitcherItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[1].tabSwitcherItem.identifier);

  // Check that the group is correctly added to the consumer.
  const TabGroup* group_0 =
      web_state_list_->CreateGroup({0}, {}, TabGroupId::GenerateNew());

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(3ul, consumer_.items.count);
  EXPECT_EQ(group_0, consumer_.items[0].tabGroupItem.tabGroup);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[1].tabSwitcherItem.identifier);
  EXPECT_EQ(group_0, consumer_.itemParents[consumer_.items[1]].tabGroup);
  EXPECT_NSEQ(group_0->GetColor(),
              consumer_.itemData[consumer_.items[1]].groupStrokeColor);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.items[2].tabSwitcherItem.identifier);
  EXPECT_NSEQ(nil, consumer_.itemData[consumer_.items[2]].groupStrokeColor);

  // Check that the closed tab and its group are removed from the consumer.
  web_state_list_->CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(1ul, consumer_.items.count);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].tabSwitcherItem.identifier);
}

// Test that `TabStripItemData` elements are updated accordingly.
TEST_F(TabStripMediatorTest, TabStripItemDataUpdated) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c* [ 0 d e ] f [ 1 g h ]"));
  for (int i = 0; i < web_state_list_->count(); ++i) {
    web::FakeWebState* web_state =
        static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(i));
    web_state->SetBrowserState(profile_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state, ios::FaviconServiceFactory::GetForProfile(
                       profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));
  }

  InitializeMediator();

  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const auto group_0_color = group_0->GetColor();
  const auto group_1_color = group_1->GetColor();

  ASSERT_EQ(10ul, consumer_.items.count);
  TabStripItemIdentifier* item_a = consumer_.items[0];
  TabStripItemIdentifier* item_b = consumer_.items[1];
  TabStripItemIdentifier* item_c = consumer_.items[2];
  TabStripItemIdentifier* item_0 = consumer_.items[3];
  TabStripItemIdentifier* item_d = consumer_.items[4];
  TabStripItemIdentifier* item_e = consumer_.items[5];
  TabStripItemIdentifier* item_f = consumer_.items[6];
  TabStripItemIdentifier* item_1 = consumer_.items[7];
  TabStripItemIdentifier* item_g = consumer_.items[8];
  TabStripItemIdentifier* item_h = consumer_.items[9];

  // 0. Testing data up-to-date after initialization.

  // Test group stroke color.
  EXPECT_NSEQ(consumer_.itemData[item_a].groupStrokeColor, nil);
  EXPECT_NSEQ(consumer_.itemData[item_b].groupStrokeColor, nil);
  EXPECT_NSEQ(consumer_.itemData[item_c].groupStrokeColor, nil);
  EXPECT_NSEQ(consumer_.itemData[item_0].groupStrokeColor, group_0_color);
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_0_color);
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, group_0_color);
  EXPECT_NSEQ(consumer_.itemData[item_f].groupStrokeColor, nil);
  EXPECT_NSEQ(consumer_.itemData[item_1].groupStrokeColor, group_1_color);
  EXPECT_NSEQ(consumer_.itemData[item_g].groupStrokeColor, group_1_color);
  EXPECT_NSEQ(consumer_.itemData[item_h].groupStrokeColor, group_1_color);

  // Test is first tab in group.
  EXPECT_EQ(consumer_.itemData[item_a].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_b].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_c].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_0].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_d].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_f].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_1].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_g].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_h].isFirstTabInGroup, NO);

  // Test is last tab in group.
  EXPECT_EQ(consumer_.itemData[item_a].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_b].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_c].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_0].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_f].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_1].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_g].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_h].isLastTabInGroup, YES);

  // 1. Testing data up-to-date after WebStateListChange::Type::kStatusOnly.

  const web::WebState* web_state_e = builder.GetWebStateForIdentifier('e');
  web_state_list_->RemoveFromGroups(
      {web_state_list_->GetIndexOfWebState(web_state_e)});
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | c* [ 0 d ] e f [ 1 g h ]");
  // Group stroke color of 'e' should now be nil, and 'd' is now the last tab of
  // its group.
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, nil);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, NO);

  web_state_list_->MoveToGroup(
      {web_state_list_->GetIndexOfWebState(web_state_e)}, group_0);
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | c* [ 0 d e ] f [ 1 g h ]");
  // Group stroke color of 'e' should be back to its previous value, and be the
  // last tab of its group.
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, group_0_color);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, YES);

  web_state_list_->DeleteGroup(group_1);
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 d e ] f g h");
  EXPECT_NSEQ(consumer_.itemData[item_g].groupStrokeColor, nil);
  EXPECT_NSEQ(consumer_.itemData[item_h].groupStrokeColor, nil);
  EXPECT_EQ(consumer_.itemData[item_g].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_h].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_g].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_h].isLastTabInGroup, NO);

  // 2. Testing data up-to-date after WebStateListChange::Type::kDetach.

  const web::WebState* web_state_d = builder.GetWebStateForIdentifier('d');
  std::unique_ptr<web::WebState> web_state_d_detached =
      web_state_list_->DetachWebStateAt(
          web_state_list_->GetIndexOfWebState(web_state_d));
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 e ] f g h");
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, YES);

  // 3. Testing data up-to-date after WebStateListChange::Type::kInsert.

  // Reset the identifier for the detached web state, as it was removed when
  // detached.
  builder.SetWebStateIdentifier(web_state_d_detached.get(), 'd');
  web_state_list_->InsertWebState(
      std::move(web_state_d_detached),
      WebStateList::InsertionParams::AtIndex(
          web_state_list_->GetIndexOfWebState(web_state_e))
          .InGroup(group_0));
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 d e ] f g h");
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_0_color);
  EXPECT_EQ(consumer_.itemData[item_d].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, NO);

  // 4. Testing data up-to-date after WebStateListChange::Type::kMove.

  web_state_list_->MoveWebStateAt(3, 4);
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 e d ] f g h");
  EXPECT_EQ(consumer_.itemData[item_d].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, NO);

  // 5. Testing data up-to-date after WebStateListChange::Type::kGroupCreate.

  const TabGroup* group_2 =
      web_state_list_->CreateGroup({web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('c')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('d')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('f')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('g')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('h'))},
                                   {}, TabGroupId::GenerateNew());
  builder.SetTabGroupIdentifier(group_2, '2');
  UIColor* group_2_color = group_2->GetColor();
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | [ 2 c* d f g h ] [ 0 e ]");
  EXPECT_NSEQ(consumer_.itemData[item_c].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_f].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_g].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_h].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, group_0_color);
  EXPECT_EQ(consumer_.itemData[item_c].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_d].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_h].isLastTabInGroup, YES);

  // 6. Testing data up-to-date after
  // WebStateListChange::Type::kGroupVisualDataUpdate.

  TabStripItemIdentifier* item_2 = consumer_.items[2];
  web_state_list_->UpdateGroupVisualData(
      group_2, {u"Updated Group Name", tab_groups::TabGroupColorId::kRed});
  group_2_color = group_2->GetColor();
  EXPECT_NSEQ(consumer_.itemData[item_2].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_c].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_f].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_g].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_h].groupStrokeColor, group_2_color);
}

// Test that parent elements are updated accordingly.
TEST_F(TabStripMediatorTest, ItemParentsUpdated) {
  WebStateListBuilderFromDescription builder(web_state_list_);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c* [ 0 d e ] f [ 1 g h ]"));
  for (int i = 0; i < web_state_list_->count(); ++i) {
    web::FakeWebState* web_state =
        static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(i));
    web_state->SetBrowserState(profile_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state, ios::FaviconServiceFactory::GetForProfile(
                       profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));
  }

  InitializeMediator();

  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');

  ASSERT_EQ(10ul, consumer_.items.count);
  TabStripItemIdentifier* item_a = consumer_.items[0];
  TabStripItemIdentifier* item_b = consumer_.items[1];
  TabStripItemIdentifier* item_c = consumer_.items[2];
  TabStripItemIdentifier* item_0 = consumer_.items[3];
  TabStripItemIdentifier* item_d = consumer_.items[4];
  TabStripItemIdentifier* item_e = consumer_.items[5];
  TabStripItemIdentifier* item_f = consumer_.items[6];
  TabStripItemIdentifier* item_1 = consumer_.items[7];
  TabStripItemIdentifier* item_g = consumer_.items[8];
  TabStripItemIdentifier* item_h = consumer_.items[9];

  // 0. Testing parents up-to-date after initialization.

  EXPECT_EQ(consumer_.itemParents[item_a].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_b].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_c].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_0].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_d].tabGroup, group_0);
  EXPECT_EQ(consumer_.itemParents[item_e].tabGroup, group_0);
  EXPECT_EQ(consumer_.itemParents[item_f].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_1].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_g].tabGroup, group_1);
  EXPECT_EQ(consumer_.itemParents[item_h].tabGroup, group_1);

  // 1. Testing parents up-to-date after WebStateListChange::Type::kStatusOnly.

  const web::WebState* web_state_e = builder.GetWebStateForIdentifier('e');
  web_state_list_->RemoveFromGroups(
      {web_state_list_->GetIndexOfWebState(web_state_e)});
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | c* [ 0 d ] e f [ 1 g h ]");
  EXPECT_EQ(consumer_.itemParents[item_e].tabGroup, nullptr);

  web_state_list_->MoveToGroup(
      {web_state_list_->GetIndexOfWebState(web_state_e)}, group_0);
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | c* [ 0 d e ] f [ 1 g h ]");
  EXPECT_EQ(consumer_.itemParents[item_e].tabGroup, group_0);

  web_state_list_->DeleteGroup(group_1);
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 d e ] f g h");
  EXPECT_EQ(consumer_.itemParents[item_g].tabGroup, nullptr);
  EXPECT_EQ(consumer_.itemParents[item_h].tabGroup, nullptr);

  // 2. Testing parents up-to-date after WebStateListChange::Type::kInsert.

  const web::WebState* web_state_d = builder.GetWebStateForIdentifier('d');
  std::unique_ptr<web::WebState> web_state_d_detached =
      web_state_list_->DetachWebStateAt(
          web_state_list_->GetIndexOfWebState(web_state_d));
  // Reset the identifier for the detached WebState.
  builder.SetWebStateIdentifier(web_state_d_detached.get(), 'd');
  web_state_list_->InsertWebState(
      std::move(web_state_d_detached),
      WebStateList::InsertionParams::AtIndex(
          web_state_list_->GetIndexOfWebState(web_state_e))
          .InGroup(group_0));
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 d e ] f g h");
  EXPECT_EQ(consumer_.itemParents[item_d].tabGroup, group_0);

  // 3. Testing parents up-to-date after WebStateListChange::Type::kMove.

  web_state_list_->MoveWebStateAt(3, 4);
  ASSERT_EQ(builder.GetWebStateListDescription(), "a b | c* [ 0 e d ] f g h");
  EXPECT_EQ(consumer_.itemParents[item_d].tabGroup, group_0);

  // 4. Testing data up-to-date after WebStateListChange::Type::kGroupCreate.

  const TabGroup* group_2 =
      web_state_list_->CreateGroup({web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('c')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('d')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('f')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('g')),
                                    web_state_list_->GetIndexOfWebState(
                                        builder.GetWebStateForIdentifier('h'))},
                                   {}, TabGroupId::GenerateNew());
  builder.SetTabGroupIdentifier(group_2, '2');
  ASSERT_EQ(builder.GetWebStateListDescription(),
            "a b | [ 2 c* d f g h ] [ 0 e ]");
  EXPECT_EQ(consumer_.itemParents[item_c].tabGroup, group_2);
  EXPECT_EQ(consumer_.itemParents[item_d].tabGroup, group_2);
  EXPECT_EQ(consumer_.itemParents[item_f].tabGroup, group_2);
  EXPECT_EQ(consumer_.itemParents[item_g].tabGroup, group_2);
  EXPECT_EQ(consumer_.itemParents[item_h].tabGroup, group_2);
  EXPECT_EQ(consumer_.itemParents[item_e].tabGroup, group_0);
}

// Tests that changing the selected tab is correctly reflected in the consumer.
TEST_F(TabStripMediatorTest, SelectTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());

  web_state_list_->ActivateWebStateAt(0);

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Check that replacing a tab in the WebStateList is reflected in the TabStrip.
TEST_F(TabStripMediatorTest, ReplacedTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());

  auto web_state = std::make_unique<web::FakeWebState>();
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state_list_->ReplaceWebStateAt(1, std::move(web_state));

  ASSERT_EQ(2, web_state_list_->count());

  EXPECT_EQ(web_state_id, consumer_.selectedItem.identifier);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[0].tabSwitcherItem.identifier);
  EXPECT_EQ(web_state_id, consumer_.items[1].tabSwitcherItem.identifier);
}

// Tests that closing a tab works.
TEST_F(TabStripMediatorTest, WebStateChange) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  // Check title update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(0))
      ->SetTitle(u"test test");
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.reconfiguredItems.firstObject.tabSwitcherItem.identifier);

  consumer_.reconfiguredItems = nil;

  // Check loading state update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(1))
      ->SetLoading(true);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reconfiguredItems.firstObject.tabSwitcherItem.identifier);

  consumer_.reconfiguredItems = nil;

  // Check loading state update.
  static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(1))
      ->SetLoading(false);
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reconfiguredItems.firstObject.tabSwitcherItem.identifier);

  consumer_.reconfiguredItems = nil;

  // Check favicon update.
  favicon::WebFaviconDriver* driver = favicon::WebFaviconDriver::FromWebState(
      web_state_list_->GetWebStateAt(1));
  driver->OnFaviconUpdated(GURL(),
                           favicon::FaviconDriverObserver::TOUCH_LARGEST,
                           GURL(), false, gfx::Image());

  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.reconfiguredItems.firstObject.tabSwitcherItem.identifier);
}

// Tests that adding a new tab works.
TEST_F(TabStripMediatorTest, AddTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  [mediator_ addNewItem];

  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(3, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);

  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state.get(), ios::FaviconServiceFactory::GetForProfile(
                           profile_.get(), ServiceAccessType::IMPLICIT_ACCESS));

  web_state_list_->InsertWebState(std::move(web_state),
                                  WebStateList::InsertionParams::AtIndex(1));

  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }
}

// Tests that activating a tab works.
TEST_F(TabStripMediatorTest, ActivateTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(0)];

  [mediator_ activateItem:item];

  EXPECT_EQ(0, web_state_list_->active_index());

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that closing a tab works.
TEST_F(TabStripMediatorTest, CloseTab) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(1)];
  [mediator_ closeItem:item];

  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_EQ(1, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that removing a tab from its group works.
TEST_F(TabStripMediatorTest, RemoveTabFromGroup) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({1, 2}, {}, TabGroupId::GenerateNew());

  InitializeMediator();

  ASSERT_EQ(4, web_state_list_->count());
  EXPECT_EQ(2, group->range().count());

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(1)];
  [mediator_ removeItemFromGroup:item];

  EXPECT_EQ(4, web_state_list_->count());
  EXPECT_EQ(1, group->range().count());
}

// Tests that closing all non-pinned tabs except a pinned tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptPinned) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c d e f* [ 1 g h ]", browser_->GetProfile()));
  web::WebState* web_state_to_keep = builder.GetWebStateForIdentifier('b');

  InitializeMediator();

  TabGroupId tab_group_id =
      builder.GetTabGroupForIdentifier('1')->tab_group_id();
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));
  EXPECT_CALL(*tab_group_sync_service_,
              RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*tab_group_sync_service_, RemoveGroup(tab_group_id)).Times(0);

  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state_to_keep];
  [mediator_ closeAllItemsExcept:item];

  ASSERT_EQ("a b* |", builder.GetWebStateListDescription());
}

// Tests that closing all non-pinned tabs except a non-active tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptNonActive) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c d e f* [ 1 g h ]", browser_->GetProfile()));
  web::WebState* web_state_to_keep = builder.GetWebStateForIdentifier('d');

  InitializeMediator();

  TabGroupId tab_group_id =
      builder.GetTabGroupForIdentifier('1')->tab_group_id();
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));
  EXPECT_CALL(*tab_group_sync_service_,
              RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*tab_group_sync_service_, RemoveGroup(tab_group_id)).Times(0);

  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state_to_keep];
  [mediator_ closeAllItemsExcept:item];

  ASSERT_EQ("a b | d*", builder.GetWebStateListDescription());
}

// Tests that closing all non-pinned tabs except an active tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptActive) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c d e f* [ 1 g h ]", browser_->GetProfile()));
  web::WebState* web_state_to_keep = builder.GetWebStateForIdentifier('f');

  InitializeMediator();

  TabGroupId tab_group_id =
      builder.GetTabGroupForIdentifier('1')->tab_group_id();
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(tab_group_id))
      .WillOnce(Return(TestSavedGroup()));
  EXPECT_CALL(*tab_group_sync_service_,
              RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*tab_group_sync_service_, RemoveGroup(tab_group_id)).Times(0);

  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state_to_keep];
  [mediator_ closeAllItemsExcept:item];

  ASSERT_EQ("a b | f*", builder.GetWebStateListDescription());
}

// Tests that closing all tabs except one grouped tab works.
TEST_F(TabStripMediatorTest, CloseTabsExceptGroupedTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "a b | c d e f* [ 1 g h ]", browser_->GetProfile()));
  web::WebState* web_state_to_keep = builder.GetWebStateForIdentifier('g');

  InitializeMediator();

  TabGroupId tab_group_id =
      builder.GetTabGroupForIdentifier('1')->tab_group_id();
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(tab_group_id)).Times(0);
  EXPECT_CALL(*tab_group_sync_service_,
              RemoveLocalTabGroupMapping(tab_group_id, _))
      .Times(0);
  EXPECT_CALL(*tab_group_sync_service_, RemoveGroup(tab_group_id)).Times(0);

  TabSwitcherItem* item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state_to_keep];
  [mediator_ closeAllItemsExcept:item];

  ASSERT_EQ("a b | [ 1 g* ]", builder.GetWebStateListDescription());
}

// Tests that moving web states works.
TEST_F(TabStripMediatorTest, MoveWebStates) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();

  InitializeMediator();

  web_state_list_->MoveWebStateAt(1, 4);
  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }

  web_state_list_->MoveWebStateAt(0, 3);
  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }

  web_state_list_->MoveWebStateAt(2, 6);
  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }

  web_state_list_->MoveWebStateAt(4, 1);
  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }

  web_state_list_->MoveWebStateAt(5, 0);
  for (int index = 0; index < web_state_list_->count(); index++) {
    EXPECT_EQ(consumer_.items[index].tabSwitcherItem.identifier,
              web_state_list_->GetWebStateAt(index)->GetUniqueIdentifier());
  }
  EXPECT_EQ(web_state_list_->count(), (int)consumer_.items.count);
}

// Tests that the consumer is correctly updated after removing all web states.
TEST_F(TabStripMediatorTest, DeleteAllWebState) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  CloseAllWebStates(*web_state_list_, WebStateList::CLOSE_NO_FLAGS);

  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();

  EXPECT_EQ(web_state_list_->count(), (int)consumer_.items.count);
}

// Tests that the appropriate command is sent when creating a new group from an
// item.
TEST_F(TabStripMediatorTest, CreateNewGroupWithItem) {
  AddWebState();
  AddWebState();

  InitializeMediator();

  const int web_state_index = 1;
  web::WebState* web_state = web_state_list_->GetWebStateAt(web_state_index);
  TabSwitcherItem* tab_switcher_item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state];
  [mediator_ createNewGroupWithItem:tab_switcher_item];
  EXPECT_EQ(std::set<web::WebStateID>{tab_switcher_item.identifier},
            tab_strip_handler_.identifiersForTabGroupCreation);
}

// Tests that the consumer is correctly updated after collapsing/expanding a
// group.
TEST_F(TabStripMediatorTest, CollapseExpandGroup) {
  AddWebState();
  AddWebState();
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({1, 2}, {}, TabGroupId::GenerateNew());
  TabGroupItem* group_item =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];
  TabStripItemIdentifier* group_item_identifier =
      [TabStripItemIdentifier groupIdentifier:group_item];

  // Ensure the group is expanded initially.
  const tab_groups::TabGroupVisualData visual_data = group->visual_data();
  const auto expanded_visual_data = tab_groups::TabGroupVisualData(
      visual_data.title(), visual_data.color(), /*is_collapsed=*/false);
  web_state_list_->UpdateGroupVisualData(group, expanded_visual_data);

  InitializeMediator();

  ASSERT_EQ(web_state_list_->count() + 1, (int)consumer_.items.count);
  EXPECT_TRUE([consumer_.expandedItems containsObject:group_item_identifier]);
  EXPECT_FALSE(group->visual_data().is_collapsed());

  // Collapsing/expanding through the mutator interface.
  [mediator_ collapseGroup:group_item];
  EXPECT_FALSE([consumer_.expandedItems containsObject:group_item_identifier]);
  EXPECT_TRUE(group->visual_data().is_collapsed());
  [mediator_ expandGroup:group_item];
  EXPECT_TRUE([consumer_.expandedItems containsObject:group_item_identifier]);
  EXPECT_FALSE(group->visual_data().is_collapsed());

  // Collapsing/expanding through the WebStateList.
  const auto collapsed_visual_data = tab_groups::TabGroupVisualData(
      visual_data.title(), visual_data.color(), /*is_collapsed=*/true);
  web_state_list_->UpdateGroupVisualData(group, collapsed_visual_data);
  EXPECT_FALSE([consumer_.expandedItems containsObject:group_item_identifier]);
  web_state_list_->UpdateGroupVisualData(group, expanded_visual_data);
  EXPECT_TRUE([consumer_.expandedItems containsObject:group_item_identifier]);
}

// Tests that the appropriate command is sent when renaming an existing group.
TEST_F(TabStripMediatorTest, RenameGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* groupItem =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];

  InitializeMediator();

  [mediator_ renameGroup:groupItem];
  EXPECT_EQ(group, tab_strip_handler_.groupForTabGroupEdition);
}

// Tests that adding a new tab in a group works.
TEST_F(TabStripMediatorTest, AddTabInGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* groupItem =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());

  [mediator_ addNewTabInGroup:groupItem];

  // Check model is updated.
  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(3, web_state_list_->count());
  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  EXPECT_EQ(group, web_state_list_->GetGroupOfWebStateAt(2));
}

// Tests that ungrouping tabs in a group works.
TEST_F(TabStripMediatorTest, UngroupTabs) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* groupItem =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());
  ASSERT_TRUE(web_state_list_->ContainsGroup(group));

  [mediator_ ungroupGroup:groupItem];

  // Check model is updated.
  EXPECT_EQ(1, web_state_list_->active_index());
  EXPECT_EQ(2, web_state_list_->count());
  EXPECT_FALSE(web_state_list_->ContainsGroup(group));
}

// Tests that deleting a group works.
TEST_F(TabStripMediatorTest, DeleteGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* groupItem =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());
  ASSERT_TRUE(web_state_list_->ContainsGroup(group));

  [mediator_ deleteGroup:groupItem];

  // Check model is updated.
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_->active_index());
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_FALSE(web_state_list_->ContainsGroup(group));
}

// Tests that closing a group works.
TEST_F(TabStripMediatorTest, CloseGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0, 1}, {}, TabGroupId::GenerateNew());
  TabGroupItem* groupItem =
      [[TabGroupItem alloc] initWithTabGroup:group
                                webStateList:web_state_list_];

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());
  ASSERT_TRUE(web_state_list_->ContainsGroup(group));

  [mediator_ closeGroup:groupItem];

  // Check model is updated.
  EXPECT_EQ(WebStateList::kInvalidIndex, web_state_list_->active_index());
  EXPECT_EQ(0, web_state_list_->count());
  EXPECT_FALSE(web_state_list_->ContainsGroup(group));
}

// Tests that adding a tab to a group works.
TEST_F(TabStripMediatorTest, AddTabToGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group =
      web_state_list_->CreateGroup({0}, {}, TabGroupId::GenerateNew());

  InitializeMediator();

  ASSERT_EQ(1, web_state_list_->active_index());
  ASSERT_EQ(2, web_state_list_->count());
  ASSERT_TRUE(web_state_list_->ContainsGroup(group));
  EXPECT_EQ(nullptr, web_state_list_->GetGroupOfWebStateAt(1));
  EXPECT_EQ(1, group->range().count());

  web::WebState* web_state_1 = web_state_list_->GetWebStateAt(1);
  TabSwitcherItem* item_for_web_state_1 =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state_1];
  [mediator_ addItem:item_for_web_state_1 toGroup:group];

  // Check model is updated.
  EXPECT_EQ(group, web_state_list_->GetGroupOfWebStateAt(1));
  EXPECT_EQ(2, group->range().count());
}

// Tests dropping an interal URL (e.g. drag from omnibox).
TEST_F(TabStripMediatorTest, DropInternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  InitializeMediator();

  GURL url_to_load = GURL(kDraggedUrl);
  id local_object = [[URLInfo alloc] initWithURL:url_to_load title:@"My title"];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  ASSERT_EQ(4, web_state_list->count());
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  EXPECT_EQ(1, web_state_list->active_index());
  EXPECT_EQ(url_to_load,
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

// Tests dropping an external URL.
TEST_F(TabStripMediatorTest, DropExternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  InitializeMediator();

  NSItemProvider* item_provider = [[NSItemProvider alloc]
      initWithContentsOfURL:[NSURL URLWithString:base::SysUTF8ToNSString(
                                                     kDraggedUrl)]];

  // Drop item.
  [mediator_ dropItemFromProvider:item_provider
                          toIndex:1
               placeholderContext:nil];

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), ^bool(void) {
        return web_state_list->count() == 4;
      }));
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  EXPECT_EQ(1, web_state_list->active_index());
  EXPECT_EQ(GURL(kDraggedUrl),
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

// Tests dropping a tab.
TEST_F(TabStripMediatorTest, DropTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  InitializeMediator();

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:0 fromSameCollection:YES];

  EXPECT_EQ("| c a* b", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests dragging the last tab out of a group.
TEST_F(TabStripMediatorTest, DropLastTabOfGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b [ 0 c ]",
                                                       browser_->GetProfile()));

  InitializeMediator();

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(2);
  tab_groups::TabGroupId group_id = group->tab_group_id();
  tab_groups::TabGroupVisualData visual_data = group->visual_data();

  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  EXPECT_CALL(*tab_group_sync_service_, RemoveLocalTabGroupMapping(group_id, _))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(group_id))
      .WillRepeatedly(Return(saved_group));

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  TabStripLastTabDraggedAlertCommand* command =
      tab_strip_handler_.lastTabDraggedCommand;
  EXPECT_EQ(web_state_id, command.tabID);
  EXPECT_EQ(browser_.get(), command.originBrowser);
  EXPECT_EQ(2, command.originIndex);
  EXPECT_EQ(visual_data, command.visualData);
  EXPECT_EQ(group_id, command.localGroupID);
  EXPECT_EQ(saved_id, command.savedGroupID);

  // The tab should still have moved.
  EXPECT_EQ("| a* c b", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests dragging the last tab out of a group from another browser.
TEST_F(TabStripMediatorTest, DropLastTabOfGroupDifferentBrowser) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b",
                                                       browser_->GetProfile()));

  WebStateList* other_web_state_list = other_browser_->GetWebStateList();
  WebStateListBuilderFromDescription other_builder(other_web_state_list);
  ASSERT_TRUE(other_builder.BuildWebStateListFromDescription(
      "| d* [ 0 e ] f", other_browser_->GetProfile()));

  InitializeMediator();

  web::WebStateID web_state_id =
      other_web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();
  const TabGroup* group = other_web_state_list->GetGroupOfWebStateAt(1);
  tab_groups::TabGroupId group_id = group->tab_group_id();
  tab_groups::TabGroupVisualData visual_data = group->visual_data();

  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  EXPECT_CALL(*tab_group_sync_service_, RemoveLocalTabGroupMapping(group_id, _))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(group_id))
      .WillRepeatedly(Return(saved_group));

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:0 fromSameCollection:NO];

  TabStripLastTabDraggedAlertCommand* command =
      tab_strip_handler_.lastTabDraggedCommand;
  EXPECT_EQ(web_state_id, command.tabID);
  EXPECT_EQ(other_browser_.get(), command.originBrowser);
  EXPECT_EQ(1, command.originIndex);
  EXPECT_EQ(visual_data, command.visualData);
  EXPECT_EQ(group_id, command.localGroupID);
  EXPECT_EQ(saved_id, command.savedGroupID);

  // The tab should still have moved.
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| c a* b", builder.GetWebStateListDescription());
  EXPECT_EQ("| d* f", other_builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOtherBrowser);
}

// Tests dragging the a tab out of a group containing several tabs.
TEST_F(TabStripMediatorTest, DropTabOutOfGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* [ 0 b c ]",
                                                       browser_->GetProfile()));

  InitializeMediator();

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(2);
  tab_groups::TabGroupId group_id = group->tab_group_id();

  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  EXPECT_CALL(*tab_group_sync_service_, GetGroup(group_id))
      .WillRepeatedly(Return(saved_group));

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  EXPECT_EQ(nil, tab_strip_handler_.lastTabDraggedCommand);

  EXPECT_EQ("| a* c [ 0 b ]", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests deleting a tab group after a move.
TEST_F(TabStripMediatorTest, DeleteTabGroup) {
  InitializeMediator();

  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*tab_group_sync_service_, RemoveGroup(saved_id)).Times(1);

  [mediator_ deleteSavedGroupWithID:saved_id];
}

// Tests cancelling a tab move on the same browser.
TEST_F(TabStripMediatorTest, CancelTabMoveSameBrowser) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  const GURL new_url = GURL("https://cancelled_url.com");
  const std::u16string new_title = u"cancelled title";
  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(1));
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state->SetVisibleURL(new_url);
  web_state->SetTitle(new_title);

  InitializeMediator();

  tab_groups::TabGroupVisualData visual_data{
      u"My title", tab_groups::TabGroupColorId::kCyan};
  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  SavedTabGroupTab saved_tab = CreateSavedTab(saved_id);
  saved_group.saved_tabs().push_back(saved_tab);

  EXPECT_CALL(*tab_group_sync_service_, GetGroup(saved_id))
      .WillOnce(Return(saved_group));

  // Make sure the service is updated with the new data.
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabGroupMapping(saved_id, local_id, _))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabId(local_id, saved_tab.saved_tab_guid(),
                               web_state_id.identifier()))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateTab(local_id, web_state_id.identifier(),
                        TabTitleAndURLEq(new_title, new_url)))
      .Times(1);

  [mediator_ cancelMoveForTab:web_state_id
                originBrowser:browser_.get()
                  originIndex:2
                   visualData:visual_data
                 localGroupID:local_id
                      savedID:saved_id];

  // The tab has moved back to its original position.
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| a* c [ 0 b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(visual_data,
            web_state_list->GetGroupOfWebStateAt(2)->visual_data());
}

// Tests cancelling a tab move on the same browser after the saved group has
// been associated with another local group.
TEST_F(TabStripMediatorTest, CancelTabMoveSameBrowserModifiedGroup) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();

  InitializeMediator();

  tab_groups::TabGroupVisualData visual_data{
      u"My title", tab_groups::TabGroupColorId::kCyan};
  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  SavedTabGroupTab saved_tab = CreateSavedTab(saved_id);
  saved_group.saved_tabs().push_back(saved_tab);

  // Associate the saved group with another local group.
  saved_group.SetLocalGroupId(tab_groups::TabGroupId::GenerateNew());

  EXPECT_CALL(*tab_group_sync_service_, GetGroup(saved_id))
      .WillOnce(Return(saved_group));

  // Make sure that no updates are done to the service.
  EXPECT_CALL(*tab_group_sync_service_, UpdateLocalTabGroupMapping(_, _, _))
      .Times(0);
  EXPECT_CALL(*tab_group_sync_service_, UpdateLocalTabId(_, _, _)).Times(0);
  EXPECT_CALL(*tab_group_sync_service_, UpdateTab(_, _, _)).Times(0);

  [mediator_ cancelMoveForTab:web_state_id
                originBrowser:browser_.get()
                  originIndex:2
                   visualData:visual_data
                 localGroupID:local_id
                      savedID:saved_id];

  // The tab hasn't moved.
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| a* b c", builder.GetWebStateListDescription());
}

// Tests cancelling a tab move on the same browser for a position that doesn't
// exist anymore.
TEST_F(TabStripMediatorTest, CancelTabMoveSameBrowserLargeIndex) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  const GURL new_url = GURL("https://cancelled_url.com");
  const std::u16string new_title = u"cancelled title";
  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(1));
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state->SetVisibleURL(new_url);
  web_state->SetTitle(new_title);

  InitializeMediator();

  tab_groups::TabGroupVisualData visual_data{
      u"My title", tab_groups::TabGroupColorId::kCyan};
  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  SavedTabGroupTab saved_tab = CreateSavedTab(saved_id);
  saved_group.saved_tabs().push_back(saved_tab);

  EXPECT_CALL(*tab_group_sync_service_, GetGroup(saved_id))
      .WillOnce(Return(saved_group));

  // Make sure the service is udpated with the new data.
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabGroupMapping(saved_id, local_id, _))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabId(local_id, saved_tab.saved_tab_guid(),
                               web_state_id.identifier()))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateTab(local_id, web_state_id.identifier(),
                        TabTitleAndURLEq(new_title, new_url)))
      .Times(1);

  // Cancel the move to a position that is larger than the number of web states.
  [mediator_ cancelMoveForTab:web_state_id
                originBrowser:browser_.get()
                  originIndex:5
                   visualData:visual_data
                 localGroupID:local_id
                      savedID:saved_id];

  // The tab has moved back to its original position.
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| a* c [ 0 b ]", builder.GetWebStateListDescription());
  EXPECT_EQ(visual_data,
            web_state_list->GetGroupOfWebStateAt(2)->visual_data());
}

// Tests cancelling a tab move on another browser.
TEST_F(TabStripMediatorTest, CancelTabMoveDifferentBrowser) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  WebStateList* other_web_state_list = other_browser_->GetWebStateList();
  WebStateListBuilderFromDescription other_builder(other_web_state_list);
  ASSERT_TRUE(other_builder.BuildWebStateListFromDescription(
      "| [ 0 a* b ] c d e", other_browser_->GetProfile()));

  const GURL new_url = GURL("https://cancelled_url.com");
  const std::u16string new_title = u"cancelled title";
  web::FakeWebState* web_state =
      static_cast<web::FakeWebState*>(web_state_list->GetWebStateAt(1));
  web::WebStateID web_state_id = web_state->GetUniqueIdentifier();
  web_state->SetVisibleURL(new_url);
  web_state->SetTitle(new_title);

  InitializeMediator();

  tab_groups::TabGroupVisualData visual_data{
      u"My title", tab_groups::TabGroupColorId::kCyan};
  const base::Uuid saved_id = base::Uuid::GenerateRandomV4();
  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup saved_group = TestSavedGroup(saved_id);
  SavedTabGroupTab saved_tab = CreateSavedTab(saved_id);
  saved_group.saved_tabs().push_back(saved_tab);

  EXPECT_CALL(*tab_group_sync_service_, GetGroup(saved_id))
      .WillOnce(Return(saved_group));

  // Make sure the service is udpated with the new data.
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabGroupMapping(saved_id, local_id, _))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateLocalTabId(local_id, saved_tab.saved_tab_guid(),
                               web_state_id.identifier()))
      .Times(1);
  EXPECT_CALL(*tab_group_sync_service_,
              UpdateTab(local_id, web_state_id.identifier(),
                        TabTitleAndURLEq(new_title, new_url)))
      .Times(1);

  // Cancel the move to a position that is larger than the number of web states.
  [mediator_ cancelMoveForTab:web_state_id
                originBrowser:other_browser_.get()
                  originIndex:3
                   visualData:visual_data
                 localGroupID:local_id
                      savedID:saved_id];

  // The tab was removed from the first web state list.
  builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| a* c", builder.GetWebStateListDescription());

  // The tab has moved to its original web state list.
  other_builder.GenerateIdentifiersForWebStateList();
  EXPECT_EQ("| [ 0 a* b ] c [ 1 f ] d e",
            other_builder.GetWebStateListDescription());
  EXPECT_EQ(visual_data,
            other_web_state_list->GetGroupOfWebStateAt(3)->visual_data());
  EXPECT_EQ(web_state, other_web_state_list->GetWebStateAt(3));
}

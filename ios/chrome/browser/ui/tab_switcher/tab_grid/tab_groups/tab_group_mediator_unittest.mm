// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class TabGroupMediatorTest : public GridMediatorTestClass {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({kTabGroupsIPad, kModernTabStrip},
                                          {});

    GridMediatorTestClass::SetUp();
    WebStateList* web_state_list = browser_->GetWebStateList();
    CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
    builder_ =
        std::make_unique<WebStateListBuilderFromDescription>(web_state_list);
    ASSERT_TRUE(builder_->BuildWebStateListFromDescription(
        "| f [ 1 a* b c ] d e ", browser_->GetProfile()));

    mode_holder_ = [[TabGridModeHolder alloc] init];

    tab_group_ = web_state_list->GetGroupOfWebStateAt(1);

    tab_group_consumer_ = OCMProtocolMock(@protocol(TabGroupConsumer));

    mediator_ = [[TabGroupMediator alloc]
        initWithWebStateList:browser_->GetWebStateList()
                    tabGroup:tab_group_->GetWeakPtr()
                    consumer:tab_group_consumer_
                gridConsumer:consumer_
                  modeHolder:mode_holder_];
    mediator_.browser = browser_.get();
  }

  void TearDown() override {
    // Forces the mediator to removes its Observer from WebStateList before the
    // Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

  // Checks that the drag item origin metric is logged in UMA.
  void ExpectThatDragItemOriginMetricLogged(DragItemOrigin origin,
                                            int count = 1) {
    histogram_tester_.ExpectUniqueSample(kUmaGroupViewDragOrigin, origin,
                                         count);
  }

 protected:
  TabGroupMediator* mediator_;
  id<TabGroupConsumer> tab_group_consumer_;
  raw_ptr<const TabGroup> tab_group_;
  std::unique_ptr<WebStateListBuilderFromDescription> builder_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
  TabGridModeHolder* mode_holder_;
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
      std::make_unique<BrowserWebStateListDelegate>());
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

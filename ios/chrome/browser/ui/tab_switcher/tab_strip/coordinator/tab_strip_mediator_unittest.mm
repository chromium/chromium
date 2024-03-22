// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon/core/favicon_url.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/history/model/history_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Fake consumer to get the passed value in tests.
@interface FakeTabStripConsumer : NSObject <TabStripConsumer>

@property(nonatomic, strong) NSMutableArray<TabStripItemIdentifier*>* items;
@property(nonatomic, strong) TabSwitcherItem* selectedItem;
@property(nonatomic, strong)
    NSArray<TabStripItemIdentifier*>* reconfiguredItems;
@property(nonatomic, strong)
    NSMutableDictionary<TabStripItemIdentifier*, TabStripItemData*>* itemData;

@end

@implementation FakeTabStripConsumer

- (void)populateWithItems:(NSArray<TabStripItemIdentifier*>*)items
             selectedItem:(TabSwitcherItem*)selectedItem
                 itemData:
                     (NSDictionary<TabStripItemIdentifier*, TabStripItemData*>*)
                         itemData {
  self.items = [items mutableCopy];
  self.selectedItem = selectedItem;
  self.itemData = [NSMutableDictionary dictionaryWithDictionary:itemData];
}

- (void)selectItem:(TabSwitcherItem*)item {
  self.selectedItem = item;
}

- (void)reconfigureItems:(NSArray<TabStripItemIdentifier*>*)items {
  self.reconfiguredItems = items;
}

- (void)moveItem:(TabSwitcherItem*)item
       afterItem:(TabSwitcherItem*)destinationItem {
  TabStripItemIdentifier* itemIdentifier =
      [TabStripItemIdentifier tabIdentifier:item];
  TabStripItemIdentifier* destinationItemIdentifier =
      [TabStripItemIdentifier tabIdentifier:destinationItem];
  [self.items removeObject:itemIdentifier];
  NSInteger destinationIndex = 0;
  if (destinationItem) {
    destinationIndex = [self.items indexOfObject:destinationItemIdentifier] + 1;
  }
  [self.items insertObject:itemIdentifier atIndex:destinationIndex];
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
         beforeItem:(TabStripItemIdentifier*)destinationItem {
  if (!destinationItem) {
    [self.items addObjectsFromArray:items];
    return;
  }
  NSInteger destinationIndex = [self.items indexOfObject:destinationItem];
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:destinationIndex++];
  }
}

- (void)removeItems:(NSArray<TabStripItemIdentifier*>*)items {
  [self.items removeObjectsInArray:items];
  [self.itemData removeObjectsForKeys:items];
}

- (void)replaceItem:(TabSwitcherItem*)oldTab withItem:(TabSwitcherItem*)newTab {
  TabStripItemIdentifier* oldItem =
      [TabStripItemIdentifier tabIdentifier:oldTab];
  TabStripItemIdentifier* newItem =
      [TabStripItemIdentifier tabIdentifier:newTab];
  NSMutableArray<TabStripItemIdentifier*>* replacedItems =
      [NSMutableArray array];
  for (NSUInteger index = 0; index < self.items.count; index++) {
    if ([self.items[index] isEqual:oldItem]) {
      [replacedItems addObject:newItem];
    } else {
      [replacedItems addObject:self.items[index]];
    }
  }
  self.items = replacedItems;
  [self.itemData removeObjectForKey:oldItem];
}

- (void)updateItemData:
            (NSDictionary<TabStripItemIdentifier*, TabStripItemData*>*)
                updatedItemData
      reconfigureItems:(BOOL)reconfigureItems {
  [self.itemData addEntriesFromDictionary:updatedItemData];
  if (reconfigureItems) {
    [self reconfigureItems:updatedItemData.allKeys];
  }
}

@end

// Test fixture for the TabStripMediator.
class TabStripMediatorTest : public PlatformTest {
 public:
  TabStripMediatorTest() {
    feature_list_.InitWithFeatures({kTabGroupsInGrid}, {});
    TestChromeBrowserState::Builder browser_state_builder;
    browser_state_builder.AddTestingFactory(
        ios::FaviconServiceFactory::GetInstance(),
        ios::FaviconServiceFactory::GetDefaultFactory());
    browser_state_builder.AddTestingFactory(
        ios::HistoryServiceFactory::GetInstance(),
        ios::HistoryServiceFactory::GetDefaultFactory());

    browser_state_ = browser_state_builder.Build();
    browser_ = std::make_unique<TestBrowser>(
        browser_state_.get(), std::make_unique<FakeWebStateListDelegate>());
    web_state_list_ = browser_->GetWebStateList();

    consumer_ = [[FakeTabStripConsumer alloc] init];
  }

  ~TabStripMediatorTest() override { [mediator_ disconnect]; }

  void InitializeMediator() {
    mediator_ = [[TabStripMediator alloc] initWithConsumer:consumer_];
    mediator_.browserState = browser_state_.get();
    mediator_.webStateList = web_state_list_;
    mediator_.browser = browser_.get();
  }

  void AddWebState(bool pinned = false) {
    auto web_state = std::make_unique<web::FakeWebState>();
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state.get(),
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));

    web_state_list_->InsertWebState(
        std::move(web_state),
        WebStateList::InsertionParams::Automatic().Activate().Pinned(pinned));
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  raw_ptr<WebStateList> web_state_list_;
  TabStripMediator* mediator_;
  FakeTabStripConsumer* consumer_;
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
  const TabGroup* group_0 = web_state_list_->CreateGroup({0}, {});

  ASSERT_NE(nil, consumer_.selectedItem);
  EXPECT_EQ(web_state_list_->GetActiveWebState()->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
  ASSERT_EQ(3ul, consumer_.items.count);
  EXPECT_EQ(group_0, consumer_.items[0].tabGroupItem.tabGroup);
  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.items[1].tabSwitcherItem.identifier);
  EXPECT_NSEQ(ColorForTabGroupColorId(group_0->visual_data().color()),
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
  WebStateListBuilderFromDescription builder;
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      *web_state_list_, "a b | c* [ 0 d e ] f [ 1 g h ]"));
  for (int i = 0; i < web_state_list_->count(); ++i) {
    web::FakeWebState* web_state =
        static_cast<web::FakeWebState*>(web_state_list_->GetWebStateAt(i));
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state,
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));
  }

  InitializeMediator();

  const TabGroup* group_0 = builder.GetTabGroupForIdentifier('0');
  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const auto group_0_color =
      ColorForTabGroupColorId(group_0->visual_data().color());
  const auto group_1_color =
      ColorForTabGroupColorId(group_1->visual_data().color());

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
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 d ] e f [ 1 g h ]");
  // Group stroke color of 'e' should now be nil, and 'd' is now the last tab of
  // its group.
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, nil);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, NO);

  web_state_list_->MoveToGroup(
      {web_state_list_->GetIndexOfWebState(web_state_e)}, group_0);
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 d e ] f [ 1 g h ]");
  // Group stroke color of 'e' should be back to its previous value, and be the
  // last tab of its group.
  EXPECT_NSEQ(consumer_.itemData[item_e].groupStrokeColor, group_0_color);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_e].isLastTabInGroup, YES);

  web_state_list_->DeleteGroup(group_1);
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 d e ] f g h");
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
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 e ] f g h");
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, YES);

  // 3. Testing data up-to-date after WebStateListChange::Type::kInsert.

  web_state_list_->InsertWebState(
      std::move(web_state_d_detached),
      WebStateList::InsertionParams::AtIndex(
          web_state_list_->GetIndexOfWebState(web_state_e))
          .InGroup(group_0));
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 d e ] f g h");
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_0_color);
  EXPECT_EQ(consumer_.itemData[item_d].isFirstTabInGroup, YES);
  EXPECT_EQ(consumer_.itemData[item_e].isFirstTabInGroup, NO);
  EXPECT_EQ(consumer_.itemData[item_d].isLastTabInGroup, NO);

  // 4. Testing data up-to-date after WebStateListChange::Type::kMove.

  web_state_list_->MoveWebStateAt(3, 4);
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
            "a b | c* [ 0 e d ] f g h");
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
                                   {});
  builder.SetTabGroupIdentifier(group_2, '2');
  UIColor* group_2_color =
      ColorForTabGroupColorId(group_2->visual_data().color());
  ASSERT_EQ(builder.GetWebStateListDescription(*web_state_list_),
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
  group_2_color = ColorForTabGroupColorId(group_2->visual_data().color());
  EXPECT_NSEQ(consumer_.itemData[item_2].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_c].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_d].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_f].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_g].groupStrokeColor, group_2_color);
  EXPECT_NSEQ(consumer_.itemData[item_h].groupStrokeColor, group_2_color);
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
  web_state->SetBrowserState(browser_state_.get());
  favicon::WebFaviconDriver::CreateForWebState(
      web_state.get(),
      ios::FaviconServiceFactory::GetForBrowserState(
          browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));

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

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(0)
                             ->GetUniqueIdentifier()];

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

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(1)
                             ->GetUniqueIdentifier()];
  [mediator_ closeItem:item];

  EXPECT_EQ(0, web_state_list_->active_index());
  EXPECT_EQ(1, web_state_list_->count());

  EXPECT_EQ(web_state_list_->GetWebStateAt(0)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that closing all non-pinned tabs except a pinned tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptPinned) {
  AddWebState(/* pinned= */ true);  // 0
  AddWebState(/* pinned= */ true);  // 1, will be kept
  const auto web_state_to_keep_identifier =
      web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier();
  AddWebState();  // 2
  AddWebState();  // 3
  AddWebState();  // 4
  AddWebState();  // 5

  InitializeMediator();

  ASSERT_EQ(5, web_state_list_->active_index());
  ASSERT_EQ(6, web_state_list_->count());

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(1)
                             ->GetUniqueIdentifier()];
  [mediator_ closeAllItemsExcept:item];

  EXPECT_EQ(1, web_state_list_->active_index());
  EXPECT_EQ(2, web_state_list_->count());

  // Check that the WebState at index 1 is the one that should have been kept.
  EXPECT_EQ(web_state_to_keep_identifier,
            web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier());
  // Check that the currently selected item is the WebState at index 1.
  EXPECT_EQ(web_state_list_->GetWebStateAt(1)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that closing all non-pinned tabs except a non-active tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptNonActive) {
  AddWebState(/* pinned= */ true);  // 0
  AddWebState(/* pinned= */ true);  // 1
  AddWebState();                    // 2
  AddWebState();                    // 3, will be kept
  const auto web_state_to_keep_identifier =
      web_state_list_->GetWebStateAt(3)->GetUniqueIdentifier();
  AddWebState();  // 4
  AddWebState();  // 5

  InitializeMediator();

  ASSERT_EQ(5, web_state_list_->active_index());
  ASSERT_EQ(6, web_state_list_->count());

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(3)
                             ->GetUniqueIdentifier()];
  [mediator_ closeAllItemsExcept:item];

  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(3, web_state_list_->count());

  // Check that the WebState at index 2 is the one that should have been kept.
  EXPECT_EQ(web_state_to_keep_identifier,
            web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier());
  // Check that the currently selected item is the WebState at index 2.
  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
}

// Tests that closing all non-pinned tabs except an active tab works.
TEST_F(TabStripMediatorTest, CloseAllNonPinnedTabsExceptActive) {
  AddWebState(/* pinned= */ true);  // 0
  AddWebState(/* pinned= */ true);  // 1
  AddWebState();                    // 2
  AddWebState();                    // 3
  AddWebState();                    // 4
  AddWebState();                    // 5, will be kept
  const auto web_state_to_keep_identifier =
      web_state_list_->GetWebStateAt(5)->GetUniqueIdentifier();

  InitializeMediator();

  ASSERT_EQ(5, web_state_list_->active_index());
  ASSERT_EQ(6, web_state_list_->count());

  TabSwitcherItem* item = [[TabSwitcherItem alloc]
      initWithIdentifier:web_state_list_->GetWebStateAt(5)
                             ->GetUniqueIdentifier()];
  [mediator_ closeAllItemsExcept:item];

  EXPECT_EQ(2, web_state_list_->active_index());
  EXPECT_EQ(3, web_state_list_->count());

  // Check that the WebState at index 2 is the one that should have been kept.
  EXPECT_EQ(web_state_to_keep_identifier,
            web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier());
  // Check that the currently selected item is the WebState at index 2.
  EXPECT_EQ(web_state_list_->GetWebStateAt(2)->GetUniqueIdentifier(),
            consumer_.selectedItem.identifier);
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

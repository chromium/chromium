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
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/tab_strip_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/favicon/favicon_url.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// Fake handler to get commands in tests.
@interface FakeTabStripHandler : NSObject <TabStripCommands>

@property(nonatomic, assign) std::set<web::WebStateID>
    identifiersForTabGroupCreation;

@property(nonatomic, assign) const TabGroup* groupForTabGroupEdition;

@end

@implementation FakeTabStripHandler

- (void)setNewTabButtonOnTabStripIPHHighlighted:(BOOL)IPHHighlighted {
}

- (void)showTabStripGroupCreationForTabs:
    (const std::set<web::WebStateID>&)identifiers {
  _identifiersForTabGroupCreation = identifiers;
}

- (void)showTabStripGroupEditionForGroup:(const TabGroup*)tabGroup {
  _groupForTabGroupEdition = tabGroup;
}

- (void)hideTabStripGroupCreation {
}

- (void)shareItem:(TabSwitcherItem*)tabSwitcherItem
       originView:(UIView*)originView {
}

@end

// Fake consumer to get the passed value in tests.
@interface FakeTabStripConsumer : NSObject <TabStripConsumer>

@property(nonatomic, strong) NSMutableArray<TabStripItemIdentifier*>* items;
@property(nonatomic, strong) TabSwitcherItem* selectedItem;
@property(nonatomic, strong)
    NSArray<TabStripItemIdentifier*>* reconfiguredItems;
@property(nonatomic, strong)
    NSMutableDictionary<TabStripItemIdentifier*, TabStripItemData*>* itemData;
@property(nonatomic, strong)
    NSMutableDictionary<TabStripItemIdentifier*, TabGroupItem*>* itemParents;
@property(nonatomic, strong)
    NSMutableSet<TabStripItemIdentifier*>* expandedItems;

@end

@implementation FakeTabStripConsumer

- (void)populateWithItems:(NSArray<TabStripItemIdentifier*>*)items
             selectedItem:(TabSwitcherItem*)selectedItem
                 itemData:
                     (NSDictionary<TabStripItemIdentifier*, TabStripItemData*>*)
                         itemData
              itemParents:
                  (NSDictionary<TabStripItemIdentifier*, TabGroupItem*>*)
                      itemParents {
  self.items = [items mutableCopy];
  self.selectedItem = selectedItem;
  self.itemData = [NSMutableDictionary dictionaryWithDictionary:itemData];
  self.itemParents = [NSMutableDictionary dictionaryWithDictionary:itemParents];
  self.expandedItems = [NSMutableSet set];
  for (TabStripItemIdentifier* item in self.items) {
    if (item.tabGroupItem && !item.tabGroupItem.collapsed) {
      [self.expandedItems addObject:item];
    }
  }
}

- (void)selectItem:(TabSwitcherItem*)item {
  self.selectedItem = item;
}

- (void)reconfigureItems:(NSArray<TabStripItemIdentifier*>*)items {
  self.reconfiguredItems = items;
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
      beforeItem:(TabStripItemIdentifier*)destinationItemIdentifier {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] beforeItem:destinationItemIdentifier];
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
       afterItem:(TabStripItemIdentifier*)destinationItemIdentifier {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] afterItem:destinationItemIdentifier];
}

- (void)moveItem:(TabStripItemIdentifier*)itemIdentifier
     insideGroup:(TabGroupItem*)destinationGroup {
  [self.items removeObject:itemIdentifier];
  [self insertItems:@[ itemIdentifier ] insideGroup:destinationGroup];
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
         beforeItem:(TabStripItemIdentifier*)destinationItem {
  int destinationIndex = self.items.count;
  if (destinationItem) {
    destinationIndex = [self.items indexOfObject:destinationItem];
  }
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:destinationIndex++];
    self.itemParents[item] = self.itemParents[destinationItem];
  }
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
          afterItem:(TabStripItemIdentifier*)destinationItem {
  if (!destinationItem) {
    NSMutableArray* newItems = [items mutableCopy];
    [newItems addObjectsFromArray:self.items];
    self.items = newItems;
    return;
  }
  NSInteger destinationIndex = [self.items indexOfObject:destinationItem] + 1;
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:destinationIndex];
    self.itemParents[item] = self.itemParents[destinationItem];
  }
}

- (void)insertItems:(NSArray<TabStripItemIdentifier*>*)items
        insideGroup:(TabGroupItem*)destinationGroup {
  if (self.items.count == 0) {
    return;
  }
  TabStripItemIdentifier* destinationGroupIdentifier =
      [TabStripItemIdentifier groupIdentifier:destinationGroup];
  // Finding the destination item: either a tab item in `destinationGroup` or
  // the group item itself.
  TabStripItemIdentifier* destinationItemIdentifier = nil;
  NSUInteger candidateDestinationItemIndex = self.items.count;
  while (candidateDestinationItemIndex > 0) {
    candidateDestinationItemIndex--;
    TabStripItemIdentifier* candidateDestinationItemIdentifier =
        self.items[candidateDestinationItemIndex];
    if ([destinationGroupIdentifier
            isEqual:candidateDestinationItemIdentifier] ||
        CompareTabGroupItems(
            destinationGroup,
            self.itemParents[candidateDestinationItemIdentifier])) {
      destinationItemIdentifier = candidateDestinationItemIdentifier;
      break;
    }
  }
  if (!destinationItemIdentifier) {
    return;
  }
  // If a destination is found, inserts the items after the destination and
  // update parent.
  candidateDestinationItemIndex += 1;
  for (TabStripItemIdentifier* item in items) {
    [self.items insertObject:item atIndex:candidateDestinationItemIndex++];
    self.itemParents[item] = destinationGroup;
  }
}

- (void)removeItems:(NSArray<TabStripItemIdentifier*>*)items {
  [self.items removeObjectsInArray:items];
  [self.itemData removeObjectsForKeys:items];
  [self.expandedItems minusSet:[NSSet setWithArray:items]];
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

- (void)collapseGroup:(TabGroupItem*)group {
  TabStripItemIdentifier* groupItemIdentifier =
      [TabStripItemIdentifier groupIdentifier:group];
  CHECK([self.expandedItems containsObject:groupItemIdentifier]);
  [self.expandedItems removeObject:groupItemIdentifier];
}

- (void)expandGroup:(TabGroupItem*)group {
  TabStripItemIdentifier* groupItemIdentifier =
      [TabStripItemIdentifier groupIdentifier:group];
  CHECK(![self.expandedItems containsObject:groupItemIdentifier]);
  [self.expandedItems addObject:groupItemIdentifier];
}

@end

// Test fixture for the TabStripMediator.
class TabStripMediatorTest : public PlatformTest {
 public:
  TabStripMediatorTest() {
    feature_list_.InitWithFeatures(
        {kTabGroupsInGrid, kTabGroupsIPad, kModernTabStrip}, {});
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

    tab_strip_handler_ = [[FakeTabStripHandler alloc] init];

    consumer_ = [[FakeTabStripConsumer alloc] init];
  }

  ~TabStripMediatorTest() override { [mediator_ disconnect]; }

  void InitializeMediator() {
    BrowserList* browserList =
        BrowserListFactory::GetForBrowserState(browser_state_.get());
    browserList->AddBrowser(browser_.get());
    mediator_ = [[TabStripMediator alloc] initWithConsumer:consumer_
                                               browserList:browserList];
    mediator_.browserState = browser_state_.get();
    mediator_.webStateList = web_state_list_;
    mediator_.browser = browser_.get();
    mediator_.tabStripHandler = tab_strip_handler_;
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
  FakeTabStripHandler* tab_strip_handler_;
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
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state,
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));
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
                                   {});
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
    web_state->SetBrowserState(browser_state_.get());
    favicon::WebFaviconDriver::CreateForWebState(
        web_state,
        ios::FaviconServiceFactory::GetForBrowserState(
            browser_state_.get(), ServiceAccessType::IMPLICIT_ACCESS));
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
                                   {});
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
  const TabGroup* group = web_state_list_->CreateGroup({1, 2}, {});

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

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(1)];
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

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(3)];
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

  TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
      initWithWebState:web_state_list_->GetWebStateAt(5)];
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
  const TabGroup* group = web_state_list_->CreateGroup({1, 2}, {});
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
  const TabGroup* group = web_state_list_->CreateGroup({0, 1}, {});
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
  const TabGroup* group = web_state_list_->CreateGroup({0, 1}, {});
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
  const TabGroup* group = web_state_list_->CreateGroup({0, 1}, {});
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
  const TabGroup* group = web_state_list_->CreateGroup({0, 1}, {});
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

// Tests that adding a tab to a group works.
TEST_F(TabStripMediatorTest, AddTabToGroup) {
  AddWebState();
  AddWebState();
  const TabGroup* group = web_state_list_->CreateGroup({0}, {});

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

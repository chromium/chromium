// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/create_tab_group_mediator.h"

#import "base/check.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_creation_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@implementation CreateTabGroupMediator {
  // Consumer of the tab group creator;
  __weak id<TabGroupCreationConsumer> _consumer;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Web state list where the tab group belong.
  WebStateList* _webStateList;
  // Tab group to edit.
  const TabGroup* _tabGroup;
  // Array of all pictures of the group.
  NSMutableArray<GroupTabInfo*>* _tabGroupInfos;
  // Item to fetch pictures.
  TabGroupItem* _groupItem;
}

- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                        webStateList:(WebStateList*)webStateList {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
    CHECK(webStateList);
    _consumer = consumer;
    [_consumer setDefaultGroupColor:TabGroup::DefaultColorForNewTabGroup(
                                        webStateList)];

    _identifiers = identifiers;
    _webStateList = webStateList;

    _tabGroupInfos = [[NSMutableArray alloc] init];

    NSUInteger numberOfRequestedImages = 0;
    for (web::WebStateID identifier : identifiers) {
      if (numberOfRequestedImages >= 7) {
        break;
      }
      // TODO(crbug.com/333032676): Replace this with the appropriate helper
      // once it exists.
      int index = GetWebStateIndex(
          webStateList, WebStateSearchCriteria{.identifier = identifier});
      CHECK_NE(index, WebStateList::kInvalidIndex);

      __weak CreateTabGroupMediator* weakSelf = self;
      [TabGroupUtils
          fetchTabGroupInfoFromWebState:webStateList->GetWebStateAt(index)
                             completion:^(GroupTabInfo* info) {
                               [weakSelf addInfo:info];
                               [weakSelf updateConsumer];
                             }];
      numberOfRequestedImages++;
    }
  }
  return self;
}

- (instancetype)initTabGroupEditionWithConsumer:
                    (id<TabGroupCreationConsumer>)consumer
                                       tabGroup:(const TabGroup*)tabGroup
                                   webStateList:(WebStateList*)webStateList {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(tabGroup);
    _consumer = consumer;
    _tabGroup = tabGroup;
    _webStateList = webStateList;
    _groupItem = [[TabGroupItem alloc] initWithTabGroup:_tabGroup
                                           webStateList:_webStateList];
    __weak CreateTabGroupMediator* weakSelf = self;
    [_groupItem fetchGroupTabInfos:^(TabGroupItem* item,
                                     NSArray<GroupTabInfo*>* groupTabInfos) {
      [weakSelf setGroupTabInfos:groupTabInfos];
      [weakSelf updateConsumer];
    }];

    // Do not use the helper to get the following values as the title helper do
    // not return nil but the number of tabs. In this case, we want nil so it do
    // not display anything.
    tab_groups::TabGroupVisualData visualData = _tabGroup->visual_data();
    [_consumer setDefaultGroupColor:visualData.color()];
    [_consumer setGroupTitle:base::SysUTF16ToNSString(visualData.title())];
  }
  return self;
}

#pragma mark - TabGroupCreationMutator

// TODO(crbug.com/40942154): Rename the function to better match what it does.
- (void)createNewGroupWithTitle:(NSString*)title
                          color:(tab_groups::TabGroupColorId)colorID
                     completion:(void (^)())completion {
  tab_groups::TabGroupVisualData visualData =
      tab_groups::TabGroupVisualData(base::SysNSStringToUTF16(title), colorID);
  if (_tabGroup) {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserUpdatedGroup"));
    if (![_tabGroup->GetRawTitle() isEqualToString:title]) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupName"));
    }
    if (![_tabGroup->GetColor()
            isEqual:TabGroup::ColorForTabGroupColorId(colorID)]) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGroupUserUpdatedGroupColor"));
    }
    _webStateList->UpdateGroupVisualData(_tabGroup, visualData);
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGroupUserCreatedNewGroup"));
    std::set<int> tabIndexes;
    for (web::WebStateID identifier : _identifiers) {
      int index = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                      .identifier = identifier,
                                                  });
      if (index != WebStateList::kInvalidIndex) {
        tabIndexes.insert(index);
      }
    }
    if (!tabIndexes.empty()) {
      _webStateList->CreateGroup(tabIndexes, visualData);
    }
  }
  completion();
}

#pragma mark - Private helpers

// Adds the given info to the GroupTabInfo array.
- (void)addInfo:(GroupTabInfo*)info {
  [_tabGroupInfos addObject:info];
}

// Sets the GroupTabInfo array with `tabGroupInfos`.
- (void)setGroupTabInfos:(NSArray<GroupTabInfo*>*)tabGroupInfos {
  _tabGroupInfos = [[NSMutableArray alloc] initWithArray:tabGroupInfos];
}

// Sends to the consumer the needed pictures and the number of items to display
// it properly.
- (void)updateConsumer {
  NSInteger numberOfItem =
      _tabGroup ? _tabGroup->range().count() : _identifiers.size();
  [_consumer setTabGroupInfos:_tabGroupInfos
        numberOfSelectedItems:numberOfItem];
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_mediator.h"

#import <memory>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/tab_groups/tab_group_color.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_info.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/create_tab_group_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_group_creation_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/web/public/web_state_id.h"

@interface CreateTabGroupMediator () <WebStateListObserving>
@end

@implementation CreateTabGroupMediator {
  // Consumer of the tab group creator;
  __weak id<TabGroupCreationConsumer> _consumer;
  // List of tabs to add to the tab group.
  std::set<web::WebStateID> _identifiers;
  // Web state list where the tab group belong.
  raw_ptr<WebStateList> _webStateList;
  // Tab group to edit.
  raw_ptr<const TabGroup> _tabGroup;
  // Array of all pictures of the group.
  NSMutableArray<GroupTabInfo*>* _tabGroupInfos;
  // Item to fetch pictures.
  TabGroupItem* _groupItem;
  // Source browser. Only set when creating a new group, not when editing an
  // existing one.
  raw_ptr<Browser> _browser;
  // Observers for WebStateList. Only set when editing an existing group,
  // when creating a new one.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
}

- (instancetype)
    initTabGroupCreationWithConsumer:(id<TabGroupCreationConsumer>)consumer
                        selectedTabs:(std::set<web::WebStateID>&)identifiers
                             browser:(Browser*)browser {
  CHECK(IsTabGroupInGridEnabled())
      << "You should not be able to create a tab group outside the Tab Groups "
         "experiment.";
  self = [super init];
  if (self) {
    CHECK(consumer);
    CHECK(!identifiers.empty()) << "Cannot create an empty tab group.";
    CHECK(browser);
    _identifiers = identifiers;

    _browser = browser;
    _webStateList = browser->GetWebStateList();
    _consumer = consumer;
    [_consumer setDefaultGroupColor:TabGroup::DefaultColorForNewTabGroup(
                                        _webStateList)];

    ProfileIOS* profile = browser->GetProfile();
    BrowserList* browserList = BrowserListFactory::GetForProfile(profile);

    _tabGroupInfos = [[NSMutableArray alloc] init];

    NSUInteger numberOfRequestedImages = 0;
    for (web::WebStateID identifier : identifiers) {
      if (numberOfRequestedImages >= 7) {
        break;
      }
      WebStateList* currentWebStateList = _webStateList;
      // TODO(crbug.com/333032676): Replace this with the appropriate helper
      // once it exists.
      int index = GetWebStateIndex(
          _webStateList, WebStateSearchCriteria{.identifier = identifier});
      if (index == WebStateList::kInvalidIndex) {
        // The user is creating a group from a long press on search result. Tab
        // search can display all tabs from the same profile at the same time.
        // The selected tab is currently in a different web state list (inactive
        // tab, or tab from another window).
        Browser* selectedTabBrowser = GetBrowserForTabWithId(
            browserList, identifier, profile->IsOffTheRecord());
        CHECK(browser);
        currentWebStateList = selectedTabBrowser->GetWebStateList();
        index =
            GetWebStateIndex(currentWebStateList,
                             WebStateSearchCriteria{.identifier = identifier});
      }

      __weak CreateTabGroupMediator* weakSelf = self;
      [TabGroupUtils
          fetchTabGroupInfoFromWebState:currentWebStateList->GetWebStateAt(
                                            index)
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
    CHECK(webStateList);
    _consumer = consumer;
    _tabGroup = tabGroup;
    _webStateList = webStateList;
    // Observe the WebStateList in the case the group disappears.
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _scopedWebStateListObservation->AddObservation(_webStateList);
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

- (void)disconnect {
  if (_tabGroup) {
    _scopedWebStateListObservation->RemoveAllObservations();
    _scopedWebStateListObservation.reset();
    _webStateListObserverBridge.reset();
    _webStateList = nullptr;
  }
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
      if (index == WebStateList::kInvalidIndex) {
        index = _webStateList->count();
        MoveTabToBrowser(identifier, _browser, index);
      }
      tabIndexes.insert(index);
    }
    if (!tabIndexes.empty()) {
      _webStateList->CreateGroup(tabIndexes, visualData,
                                 tab_groups::TabGroupId::GenerateNew());
    }
  }
  completion();
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  CHECK_EQ(_webStateList, webStateList);
  switch (change.type()) {
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataUpdate =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      if (_tabGroup == visualDataUpdate.updated_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& deletion =
          change.As<WebStateListChangeGroupDelete>();
      if (_tabGroup == deletion.deleted_group()) {
        // Dismiss the editor.
        [self.delegate
            createTabGroupMediatorEditedGroupWasExternallyMutated:self];
      }
      break;
    }
    default:
      // No-op.
      break;
  }
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

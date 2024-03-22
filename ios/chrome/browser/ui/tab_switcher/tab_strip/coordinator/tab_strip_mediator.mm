// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/coordinator/tab_strip_mediator.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <set>

#import "base/metrics/histogram_functions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/tab_groups/tab_group_color.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/main/model/browser_util.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/tabs/model/features.h"
#import "ios/chrome/browser/ui/tab_switcher/group_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_features_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/web_state_list/model/web_state_list_favicon_driver_observer.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/image/image.h"

namespace {

// Returns the `TabStripItemData` for a tab item at `index` in `web_state_list`.
TabStripItemData* CreateTabItemData(int index,
                                    const WebStateList& web_state_list) {
  const TabGroup* group = web_state_list.GetGroupOfWebStateAt(index);
  TabStripItemData* data = [[TabStripItemData alloc] init];
  if (group) {
    const WebStateList::Range range = web_state_list.GetGroupRange(group);
    data.isFirstTabInGroup = range.range_begin() == index;
    data.isLastTabInGroup = range.range_end() == index + 1;
    data.groupStrokeColor =
        ColorForTabGroupColorId(group->visual_data().color());
  }
  return data;
}

// Returns the `TabStripItemData` for `group`.
TabStripItemData* CreateGroupItemData(const TabGroup* group) {
  TabStripItemData* data = [[TabStripItemData alloc] init];
  data.groupStrokeColor = ColorForTabGroupColorId(group->visual_data().color());
  return data;
}

// Returns the `TabStripItemData` elements for WebStates in `range` in
// `web_state_list`.
// If `include_groups` is true, then each output element associated with a
// WebState which is the first of its group will be preceded by an element
// associated with that group.
NSArray<TabStripItemData*>* CreateItemData(
    const WebStateList& web_state_list,
    WebStateList::Range range = WebStateList::Range::InvalidRange(),
    bool include_groups = true) {
  if (!range.IsValid()) {
    range = {0, web_state_list.count()};
  }
  CHECK_GE(range.range_begin(), 0);
  CHECK_LE(range.range_end(), web_state_list.count());
  NSMutableArray<TabStripItemData*>* data = [[NSMutableArray alloc] init];
  for (int i = range.range_begin(); i < range.range_end(); i++) {
    TabStripItemData* tab_item_data = CreateTabItemData(i, web_state_list);
    const TabGroup* group = web_state_list.GetGroupOfWebStateAt(i);
    if (include_groups && group &&
        web_state_list.GetGroupRange(group).range_begin() == i &&
        [TabStripFeaturesUtils isModernTabStripWithTabGroups]) {
      [data addObject:CreateGroupItemData(group)];
    }
    [data addObject:tab_item_data];
  }
  return data;
}

TabStripItemIdentifier* CreateTabItemIdentifier(web::WebState* web_state) {
  TabSwitcherItem* tab_item =
      [[WebStateTabSwitcherItem alloc] initWithWebState:web_state];
  TabStripItemIdentifier* tab_item_identifier =
      [TabStripItemIdentifier tabIdentifier:tab_item];
  return tab_item_identifier;
}

TabStripItemIdentifier* CreateGroupItemIdentifier(
    const TabGroup* group,
    WebStateList* web_state_list) {
  TabGroupItem* group_item =
      [[TabGroupItem alloc] initWithTabGroup:group webStateList:web_state_list];
  TabStripItemIdentifier* group_item_identifier =
      [TabStripItemIdentifier groupIdentifier:group_item];
  return group_item_identifier;
}

// Returns the `TabStripItemIdentifier` elements for WebStates in `range` in
// `web_state_list`.
// If `include_groups` is true, then each output element associated with a
// WebState which is the first of its group will be preceded by an element
// associated with that group.
NSArray<TabStripItemIdentifier*>* CreateItemIdentifiers(
    WebStateList* web_state_list,
    WebStateList::Range range = WebStateList::Range::InvalidRange(),
    bool include_groups = true) {
  if (!range.IsValid()) {
    range = {0, web_state_list->count()};
  }
  CHECK_GE(range.range_begin(), 0);
  CHECK_LE(range.range_end(), web_state_list->count());
  NSMutableArray<TabStripItemIdentifier*>* item_identifiers =
      [[NSMutableArray alloc] init];
  for (int i = range.range_begin(); i < range.range_end(); i++) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    const TabGroup* group = web_state_list->GetGroupOfWebStateAt(i);
    if (include_groups && group &&
        web_state_list->GetGroupRange(group).range_begin() == i &&
        [TabStripFeaturesUtils isModernTabStripWithTabGroups]) {
      [item_identifiers
          addObject:CreateGroupItemIdentifier(group, web_state_list)];
    }
    [item_identifiers addObject:CreateTabItemIdentifier(web_state)];
  }
  return item_identifiers;
}

}  // namespace

@interface TabStripMediator () <CRWWebStateObserver,
                                WebStateFaviconDriverObserver,
                                WebStateListObserving> {
  // Bridge C++ WebStateListObserver methods to this TabStripController.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
  // Bridge C++ WebStateObserver methods to this TabStripController.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  // Forward observer methods for all WebStates in the WebStateList monitored
  // by the TabStripMediator.
  std::unique_ptr<AllWebStateObservationForwarder>
      _allWebStateObservationForwarder;
  // Bridges FaviconDriverObservers methods to this mediator, and maintains a
  // FaviconObserver for each all webstates.
  std::unique_ptr<WebStateListFaviconDriverObserver>
      _webStateListFaviconObserver;

  // ItemID of the dragged tab. Used to check if the dropped tab is from the
  // same Chrome window.
  web::WebStateID _dragItemID;
}

// The consumer for this object.
@property(nonatomic, weak) id<TabStripConsumer> consumer;

@end

@implementation TabStripMediator

- (instancetype)initWithConsumer:(id<TabStripConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
  }
  return self;
}

- (void)disconnect {
  if (_webStateList) {
    [self removeWebStateObservations];
    _webStateListFaviconObserver.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

#pragma mark - Public properties

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    [self removeWebStateObservations];
    _webStateListFaviconObserver.reset();
    _webStateList->RemoveObserver(_webStateListObserver.get());
  }

  _webStateList = webStateList;

  if (_webStateList) {
    DCHECK_GE(_webStateList->count(), 0);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());

    _webStateListFaviconObserver =
        std::make_unique<WebStateListFaviconDriverObserver>(_webStateList,
                                                            self);

    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    [self addWebStateObservations];
  }

  [self populateConsumerItems];
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      // The activation is handled after this switch statement.
      const WebStateListChangeStatusOnly& statusOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      const int index = statusOnlyChange.index();
      const TabGroup* oldGroup = statusOnlyChange.old_group();
      const TabGroup* newGroup = statusOnlyChange.new_group();
      // WebState changed group, it should be reloaded so the group stroke is
      // updated.
      if (oldGroup != newGroup) {
        web::WebState* webState = webStateList->GetWebStateAt(index);
        TabStripItemIdentifier* itemIdentifier =
            CreateTabItemIdentifier(webState);
        TabStripItemData* itemData = CreateTabItemData(index, *webStateList);
        [self.consumer updateItemData:@{itemIdentifier : itemData}
                     reconfigureItems:YES];
        // Reconfigure the old and new group items if needed.
        if (oldGroup) {
          [self updateDataAndReconfigureItemsInGroup:oldGroup];
        }
        if (newGroup) {
          [self updateDataAndReconfigureItemsInGroup:newGroup];
        }
      }
      break;
    }
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      web::WebState* detachedWebState = detachChange.detached_web_state();
      TabStripItemIdentifier* item = [TabStripItemIdentifier
          tabIdentifier:[[WebStateTabSwitcherItem alloc]
                            initWithWebState:detachedWebState]];
      [self.consumer removeItems:@[ item ]];
      // Reconfigure the group items if needed.
      const TabGroup* group = detachChange.group();
      if (group) {
        [self updateDataAndReconfigureItemsInGroup:group];
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      const int index = insertChange.index();
      web::WebState* insertedWebState = insertChange.inserted_web_state();
      TabStripItemIdentifier* insertedItemIdentifier =
          CreateTabItemIdentifier(insertedWebState);
      TabStripItemData* insertedItemData =
          CreateTabItemData(index, *webStateList);
      [self.consumer updateItemData:@{insertedItemIdentifier : insertedItemData}
                   reconfigureItems:NO];

      TabStripItemIdentifier* destinationItemIdentifier = nil;
      if (webStateList->ContainsIndex(index + 1)) {
        const int destinationWebStateIndex = index + 1;
        web::WebState* destinationWebState =
            webStateList->GetWebStateAt(destinationWebStateIndex);
        destinationItemIdentifier =
            CreateTabItemIdentifier(destinationWebState);
      }
      [self.consumer insertItems:@[ insertedItemIdentifier ]
                      beforeItem:destinationItemIdentifier];
      // Reconfigure the group items if needed.
      if (insertChange.group()) {
        [self updateDataAndReconfigureItemsInGroup:insertChange.group()];
      }
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();
      const int to_index = moveChange.moved_to_index();
      TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
          initWithWebState:moveChange.moved_web_state()];

      const TabGroup* oldGroup = moveChange.old_group();
      const TabGroup* newGroup = moveChange.new_group();
      // WebState changed group, so the group stroke is updated.
      if (oldGroup != newGroup) {
        TabStripItemIdentifier* itemIdentifier =
            [TabStripItemIdentifier tabIdentifier:item];
        TabStripItemData* itemData = CreateGroupItemData(newGroup);
        [self.consumer updateItemData:@{itemIdentifier : itemData}
                     reconfigureItems:YES];
      }

      if (moveChange.moved_to_index() == 0) {
        [_consumer moveItem:item afterItem:nil];
      } else {
        web::WebState* destinationWebState =
            _webStateList->GetWebStateAt(to_index - 1);
        TabSwitcherItem* destinationItem = [[WebStateTabSwitcherItem alloc]
            initWithWebState:destinationWebState];
        [_consumer moveItem:item afterItem:destinationItem];
      }
      // Reconfigure the old and new group items if needed.
      if (moveChange.old_group()) {
        [self updateDataAndReconfigureItemsInGroup:moveChange.old_group()];
      }
      if (moveChange.new_group()) {
        [self updateDataAndReconfigureItemsInGroup:moveChange.new_group()];
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      const int index = replaceChange.index();
      TabSwitcherItem* oldItem = [[WebStateTabSwitcherItem alloc]
          initWithWebState:replaceChange.replaced_web_state()];
      web::WebState* newWebState = replaceChange.inserted_web_state();
      TabSwitcherItem* newItem =
          [[WebStateTabSwitcherItem alloc] initWithWebState:newWebState];
      TabStripItemIdentifier* newItemIdentifier =
          [TabStripItemIdentifier tabIdentifier:newItem];
      TabStripItemData* newItemData = CreateTabItemData(index, *webStateList);
      [self.consumer updateItemData:@{newItemIdentifier : newItemData}
                   reconfigureItems:NO];
      [self.consumer replaceItem:oldItem withItem:newItem];
      break;
    }
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& groupCreateChange =
          change.As<WebStateListChangeGroupCreate>();
      const TabGroup* group = groupCreateChange.created_group();
      TabStripItemIdentifier* groupItemIdentifier =
          CreateGroupItemIdentifier(group, webStateList);
      TabStripItemData* groupItemData = CreateGroupItemData(group);
      [self.consumer updateItemData:@{groupItemIdentifier : groupItemData}
                   reconfigureItems:NO];
      const int pivotIndex = webStateList->GetGroupRange(group).range_begin();
      TabStripItemIdentifier* pivotItemIdentifier = nil;
      if (webStateList->ContainsIndex(pivotIndex)) {
        pivotItemIdentifier =
            CreateTabItemIdentifier(webStateList->GetWebStateAt(pivotIndex));
      }
      [self.consumer insertItems:@[ groupItemIdentifier ]
                      beforeItem:pivotItemIdentifier];
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      [self updateDataAndReconfigureItemsInGroup:visualDataChange
                                                     .updated_group()];
      break;
    }
    case WebStateListChange::Type::kGroupMove: {
      // TODO(crbug.com/329810882): Handle the tab group move.
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();
      const TabGroup* group = groupDeleteChange.deleted_group();
      TabStripItemIdentifier* groupItemIdentifier =
          CreateGroupItemIdentifier(group, webStateList);
      [self.consumer removeItems:@[ groupItemIdentifier ]];
      break;
    }
  }

  if (status.active_web_state_change()) {
    // If the selected index changes as a result of the last webstate being
    // detached, the active index will be -1.
    if (webStateList->active_index() == WebStateList::kInvalidIndex) {
      [self.consumer selectItem:nil];
      return;
    }

    TabSwitcherItem* item = [[WebStateTabSwitcherItem alloc]
        initWithWebState:status.new_active_web_state];
    [self.consumer selectItem:item];
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self removeWebStateObservations];
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  [self addWebStateObservations];
  [self populateConsumerItems];
}

#pragma mark - TabStripMutator

- (void)addNewItem {
  if (!self.webStateList)
    return;

  if (!self.browserState) {
    return;
  }

  if (!IsAddNewTabAllowedByPolicy(self.browserState->GetPrefs(),
                                  self.browserState->IsOffTheRecord())) {
    return;
  }

  web::WebState::CreateParams params(self.browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  GURL url(kChromeUINewTabURL);
  web::NavigationManager::WebLoadParams loadParams(url);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  self.webStateList->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::Automatic().Activate());
  TabSwitcherItem* item;
  if (self.webStateList->GetActiveWebState()) {
    item = [[WebStateTabSwitcherItem alloc]
        initWithWebState:self.webStateList->GetActiveWebState()];
  }
  [self.consumer selectItem:item];
}

- (void)activateItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  int index =
      GetWebStateIndex(self.webStateList, WebStateSearchCriteria{
                                              .identifier = item.identifier,
                                          });

  _webStateList->ActivateWebStateAt(index);
}

- (void)closeItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }

  int index = GetWebStateIndex(
      self.webStateList,
      WebStateSearchCriteria{
          .identifier = item.identifier,
          .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned,
      });
  if (index >= 0)
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (void)closeAllItemsExcept:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  auto indexToKeepSearchCriteria = WebStateSearchCriteria(item.identifier);
  // Closes all non-pinned items except for `item`.
  CloseOtherWebStates(
      self.webStateList,
      GetWebStateIndex(self.webStateList, indexToKeepSearchCriteria),
      WebStateList::CLOSE_USER_ACTION);
}

- (void)createNewGroupWithItem:(TabSwitcherItem*)item {
  if (!self.webStateList) {
    return;
  }
  const WebStateSearchCriteria indexToAddToNewGroupSearchCriteria(
      item.identifier);
  const int indexToAddToNewGroup =
      GetWebStateIndex(self.webStateList, indexToAddToNewGroupSearchCriteria);
  // TODO(crbug.com/1501837): Update name/color.
  self.webStateList->CreateGroup(
      {indexToAddToNewGroup},
      tab_groups::TabGroupVisualData{u"Temporary Group Name",
                                     tab_groups::TabGroupColorId::kGrey});
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  if (IsVisibleURLNewTabPage(webState)) {
    return;
  }

  [self.consumer reconfigureItems:@[ CreateTabItemIdentifier(webState) ]];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self.consumer reconfigureItems:@[ CreateTabItemIdentifier(webState) ]];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self.consumer reconfigureItems:@[ CreateTabItemIdentifier(webState) ]];
}

#pragma mark - WebStateFaviconDriverObserver

- (void)faviconDriver:(favicon::FaviconDriver*)driver
    didUpdateFaviconForWebState:(web::WebState*)webState {
  [self.consumer reconfigureItems:@[ CreateTabItemIdentifier(webState) ]];
}

#pragma mark - TabCollectionDragDropHandler

- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item {
  web::WebState* webState =
      GetWebState(_webStateList, WebStateSearchCriteria{
                                     .identifier = item.identifier,
                                 });
  return CreateTabDragItem(webState);
}

- (void)dragWillBeginForItem:(TabSwitcherItem*)item {
  _dragItemID = item.identifier;
}

- (void)dragSessionDidEnd {
  _dragItemID = web::WebStateID();
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session {
  UIDragItem* dragItem = session.localDragSession.items.firstObject;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);

    if (_browserState->IsOffTheRecord() == tabInfo.incognito) {
      return UIDropOperationMove;
    }

    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }

  // All URLs originating from Chrome create a new tab (as opposed to moving a
  // tab).
  if ([dragItem.localObject isKindOfClass:[NSURL class]]) {
    return UIDropOperationCopy;
  }

  // URLs are accepted when drags originate from outside Chrome.
  NSArray<NSString*>* acceptableTypes = @[ UTTypeURL.identifier ];
  if ([session hasItemsConformingToTypeIdentifiers:acceptableTypes]) {
    return UIDropOperationCopy;
  }

  // Other UTI types such as image data or file data cannot be dropped.
  return UIDropOperationForbidden;
}

- (void)dropItem:(UIDragItem*)dragItem
               toIndex:(NSUInteger)destinationIndex
    fromSameCollection:(BOOL)fromSameCollection {
  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (!fromSameCollection) {
      // The tab lives in another Browser.
      // TODO(crbug.com/1515821): Need to be updated for pinned tabs.
      base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                    DragItemOrigin::kOtherBrwoser);
      MoveTabToBrowser(tabInfo.tabID, self.browser, destinationIndex);
    } else {
      base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    }

    // Reorder tab within same grid.
    [self moveItemWithID:tabInfo.tabID toIndex:destinationIndex];
    return;
  }
  base::UmaHistogramEnumeration(kUmaTabStripViewDragOrigin,
                                DragItemOrigin::kOther);

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewItemAtIndex:destinationIndex withURL:droppedURL.URL];
    return;
  }
}

- (void)dropItemFromProvider:(NSItemProvider*)itemProvider
                     toIndex:(NSUInteger)destinationIndex
          placeholderContext:
              (id<UICollectionViewDropPlaceholderContext>)placeholderContext {
  if (![itemProvider canLoadObjectOfClass:[NSURL class]]) {
    [placeholderContext deletePlaceholder];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto loadHandler =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [placeholderContext deletePlaceholder];
          NSURL* droppedURL = static_cast<NSURL*>(providedItem);
          [weakSelf insertNewItemAtIndex:destinationIndex
                                 withURL:net::GURLWithNSURL(droppedURL)];
        });
      };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

- (NSArray<UIDragItem*>*)allSelectedDragItems {
  NOTREACHED_NORETURN() << "You should not be able to drag and drop multiple "
                           "items. There is no selection mode in tab strip.";
}

#pragma mark - Private

// Adds an observation to every WebState of the current WebSateList.
- (void)addWebStateObservations {
  _allWebStateObservationForwarder =
      std::make_unique<AllWebStateObservationForwarder>(
          _webStateList, _webStateObserver.get());
}

// Removes an observation from every WebState of the current WebSateList.
- (void)removeWebStateObservations {
  _allWebStateObservationForwarder.reset();
}

// Updates the consumer with the list of all items and the selected one.
- (void)populateConsumerItems {
  if (!_webStateList) {
    return;
  }
  TabSwitcherItem* item;
  if (_webStateList->GetActiveWebState()) {
    item = [[WebStateTabSwitcherItem alloc]
        initWithWebState:_webStateList->GetActiveWebState()];
  }
  NSArray<TabStripItemData*>* data = CreateItemData(*_webStateList);
  NSArray<TabStripItemIdentifier*>* itemIdentifiers =
      CreateItemIdentifiers(_webStateList);
  NSDictionary<TabStripItemIdentifier*, TabStripItemData*>* itemData =
      [NSDictionary dictionaryWithObjects:data forKeys:itemIdentifiers];
  [self.consumer populateWithItems:itemIdentifiers
                      selectedItem:item
                          itemData:itemData];
}

// Moves item to the `destinationIndex`.
- (void)moveItemWithID:(web::WebStateID)itemID
               toIndex:(NSUInteger)destinationIndex {
  int sourceIndex = GetWebStateIndex(_webStateList, WebStateSearchCriteria{
                                                        .identifier = itemID,
                                                    });
  if (sourceIndex != WebStateList::kInvalidIndex) {
    int destinationWebStateListIndex =
        [self webStateListIndexFromItemIndex:destinationIndex];
    _webStateList->MoveWebStateAt(sourceIndex, destinationWebStateListIndex);
  }
}

// Inserts a new item with the given`newTabURL` at `index`.
- (void)insertNewItemAtIndex:(NSUInteger)index withURL:(const GURL&)newTabURL {
  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (_webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }
  DCHECK(_browserState);

  web::WebState::CreateParams params(_browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);

  web::NavigationManager::WebLoadParams loadParams(newTabURL);
  loadParams.transition_type = ui::PAGE_TRANSITION_TYPED;
  webState->GetNavigationManager()->LoadURLWithParams(loadParams);

  int webStateListIndex = [self webStateListIndexFromItemIndex:index];

  _webStateList->InsertWebState(
      std::move(webState),
      WebStateList::InsertionParams::AtIndex(webStateListIndex).Activate());
}

// Converts the collection view's item index to WebStateList index.
// Returns `kInvalidIndex` if `index` is out of range.
- (int)webStateListIndexFromItemIndex:(NSUInteger)index {
  // TODO(crbug.com/1515821): Need to be updated for pinned tabs.
  CHECK(!IsPinnedTabsEnabled());
  return index;
}

// For each WebState in `group`, the associated item is reconfigured with an
// up-to-date `TabStripItemData`.
- (void)updateDataAndReconfigureItemsInGroup:(const TabGroup*)group {
  if (!_webStateList) {
    return;
  }
  const WebStateList::Range range = _webStateList->GetGroupRange(group);
  NSArray<TabStripItemIdentifier*>* itemIdentifiers =
      CreateItemIdentifiers(_webStateList, range);
  NSArray<TabStripItemData*>* itemData = CreateItemData(*_webStateList, range);
  NSDictionary<TabStripItemIdentifier*, TabStripItemData*>* updatedItemData =
      [NSDictionary dictionaryWithObjects:itemData forKeys:itemIdentifiers];
  [self.consumer updateItemData:updatedItemData reconfigureItems:YES];
}

@end

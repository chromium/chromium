// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <memory>

#import "base/debug/dump_without_crashing.h"
#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/scoped_multi_source_observation.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/reading_list/model/reading_list_browser_agent.h"
#import "ios/chrome/browser/saved_tab_groups/model/ios_tab_group_sync_util.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/browser_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_grid_toolbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/tab_groups_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service.h"
#import "ios/chrome/browser/tabs_search/model/tabs_search_service_factory.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_delegate.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_toolbars_mutator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_idle_status_handler.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_observing.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_action_type.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "ui/gfx/image/image.h"

using PinnedState = WebStateSearchCriteria::PinnedState;

namespace {

void LogPriceDropMetrics(web::WebState* web_state) {
  ShoppingPersistedDataTabHelper* shopping_helper =
      ShoppingPersistedDataTabHelper::FromWebState(web_state);
  if (!shopping_helper) {
    return;
  }
  const ShoppingPersistedDataTabHelper::PriceDrop* price_drop =
      shopping_helper->GetPriceDrop();
  BOOL has_price_drop =
      price_drop && price_drop->current_price && price_drop->previous_price;
  base::RecordAction(base::UserMetricsAction(
      base::StringPrintf("Commerce.TabGridSwitched.%s",
                         has_price_drop ? "HasPriceDrop" : "NoPriceDrop")
          .c_str()));
}

// Returns the Browser with `identifier` in its WebStateList. Returns `nullptr`
// if not found.
Browser* GetBrowserForNonPinnedTabWithId(BrowserList* browser_list,
                                         web::WebStateID identifier,
                                         bool is_otr_tab) {
  const BrowserList::BrowserType browser_types =
      is_otr_tab ? BrowserList::BrowserType::kIncognito
                 : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);
  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    int index = GetWebStateIndex(web_state_list,
                                 WebStateSearchCriteria{
                                     .identifier = identifier,
                                     .pinned_state = PinnedState::kNonPinned,
                                 });
    if (index != WebStateList::kInvalidIndex) {
      return browser;
    }
  }
  return nullptr;
}

}  // namespace

@interface BaseGridMediator () <CRWWebStateObserver,
                                SnapshotStorageObserver,
                                TabGridModeObserving>
// The profile from the browser.
@property(nonatomic, readonly) ProfileIOS* profile;

@end

@implementation BaseGridMediator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>
      _scopedWebStateListObservation;
  // Observer for WebStates.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<
      base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>>
      _scopedWebStateObservation;

  // The current Browser.
  base::WeakPtr<Browser> _browser;

  // Items selected for editing.
  SelectedGridItems* _selectedEditingItems;

  // Holder for the current mode of the Tab Grid.
  TabGridModeHolder* _modeHolder;
}

- (instancetype)initWithModeHolder:(TabGridModeHolder*)modeHolder {
  if ((self = [super init])) {
    CHECK(modeHolder);
    _modeHolder = modeHolder;
    [modeHolder addObserver:self];
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObservation = std::make_unique<
        base::ScopedMultiSourceObservation<WebStateList, WebStateListObserver>>(
        _webStateListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObservation =
        std::make_unique<base::ScopedMultiSourceObservation<
            web::WebState, web::WebStateObserver>>(
            _webStateObserverBridge.get());
  }
  return self;
}

#pragma mark - Public properties

- (Browser*)browser {
  return _browser.get();
}

- (void)setBrowser:(Browser*)browser {
  [self.snapshotStorage removeObserver:self];
  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();

  _browser.reset();
  if (browser) {
    _browser = browser->AsWeakPtr();
  }

  _webStateList = browser ? browser->GetWebStateList() : nullptr;
  _profile = browser ? browser->GetProfile() : nullptr;
  _URLLoader = browser ? UrlLoadingBrowserAgent::FromBrowser(browser) : nullptr;

  [self.snapshotStorage addObserver:self];

  if (_webStateList) {
    _scopedWebStateListObservation->AddObservation(_webStateList);
    [self addWebStateObservations];
    _selectedEditingItems =
        [[SelectedGridItems alloc] initWithWebStateList:_webStateList];

    if (self.webStateList->count() > 0) {
      [self populateConsumerItems];
    }
  }
}

- (void)setConsumer:(id<TabCollectionConsumer>)consumer {
  _consumer = consumer;
  [self resetToAllItems];
  [consumer setTabGridMode:_modeHolder.mode];
}

#pragma mark - Subclassing

- (TabGridModeHolder*)modeHolder {
  return _modeHolder;
}

- (void)disconnect {
  _browser.reset();
  _profile = nil;
  _URLLoader = nil;
  _consumer = nil;
  _delegate = nil;
  _toolbarsMutator = nil;
  _containedGridToolbarsProvider = nil;
  _tabGridHandler = nil;
  _gridConsumer = nil;
  _tabPresentationDelegate = nil;

  _scopedWebStateListObservation->RemoveAllObservations();
  _scopedWebStateObservation->RemoveAllObservations();
  _scopedWebStateObservation.reset();
  _scopedWebStateListObservation.reset();

  _webStateObserverBridge.reset();
  _webStateList->RemoveObserver(_webStateListObserverBridge.get());
  _webStateListObserverBridge.reset();
  _webStateList = nil;

  [_modeHolder removeObserver:self];
  _modeHolder = nil;
}

- (void)configureToolbarsButtons {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)configureButtonsInSelectionMode:
    (TabGridToolbarsConfiguration*)configuration {
  NSUInteger selectedItemsCount = _selectedEditingItems.tabsCount;
  NSUInteger selectedShareableItemsCount =
      _selectedEditingItems.sharableTabsCount;

  BOOL allItemsSelected =
      static_cast<int>(selectedItemsCount) ==
      (self.webStateList->count() - self.webStateList->pinned_tabs_count());

  configuration.selectAllButton = !allItemsSelected;
  configuration.deselectAllButton = allItemsSelected;
  configuration.doneButton = YES;
  configuration.closeSelectedTabsButton = selectedItemsCount > 0;
  configuration.shareButton = selectedShareableItemsCount > 0;
  if (IsTabGroupInGridEnabled()) {
    configuration.addToButton = selectedItemsCount > 0;
  } else {
    configuration.addToButton = selectedShareableItemsCount > 0;
  }
  configuration.selectedItemsCount = selectedItemsCount;

  configuration.addToButtonMenu =
      [UIMenu menuWithChildren:[self addToButtonMenuElements]];
}

- (void)displayActiveTab {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)populateConsumerItems {
  if (!self.webStateList) {
    return;
  }
  [self.consumer populateItems:CreateItems(self.webStateList)
        selectedItemIdentifier:[self activeIdentifier]];
}

- (GridItemIdentifier*)activeIdentifier {
  WebStateList* webStateList = self.webStateList;
  if (!webStateList) {
    return nil;
  }

  int webStateIndex = webStateList->active_index();
  if (webStateIndex == WebStateList::kInvalidIndex) {
    return nil;
  }

  if (IsTabGroupInGridEnabled()) {
    const TabGroup* group = webStateList->GetGroupOfWebStateAt(webStateIndex);
    if (group) {
      return [GridItemIdentifier groupIdentifier:group
                                withWebStateList:webStateList];
    }
  }

  return [GridItemIdentifier
      tabIdentifier:webStateList->GetWebStateAt(webStateIndex)];
}

- (void)updateForTabInserted {
  // Default implementation is a no-op.
}

- (void)addWebStateObservations {
  int firstIndex =
      IsPinnedTabsEnabled() ? self.webStateList->pinned_tabs_count() : 0;
  for (int i = firstIndex; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    [self addObservationForWebState:webState];
  }
}

- (void)addObservationForWebState:(web::WebState*)webState {
  _scopedWebStateObservation->AddObservation(webState);
}

- (void)removeObservationForWebState:(web::WebState*)webState {
  _scopedWebStateObservation->RemoveObservation(webState);
}

- (void)insertNewWebStateAtGridIndex:(int)index withURL:(const GURL&)newTabURL {
  // The incognito mediator's Browser is briefly set to nil after the last
  // incognito tab is closed.  This occurs because the incognito profile
  // needs to be destroyed to correctly clear incognito browsing data.  Don't
  // attempt to create a new WebState with a nil profile.
  if (!self.browser) {
    return;
  }

  // There are some circumstances where a new tab insertion can be erroniously
  // triggered while another web state list mutation is happening. To ensure
  // those bugs don't become crashes, check that the web state list is OK to
  // mutate.
  if (self.webStateList->IsMutating()) {
    // Shouldn't have happened!
    DCHECK(false) << "Reentrant web state insertion!";
    return;
  }

  CHECK(self.profile);
  CHECK(self.URLLoader);

  int webStateListIndex =
      WebStateIndexFromGridDropItemIndex(self.webStateList, index);
  webStateListIndex = std::clamp(webStateListIndex, 0, _webStateList->count());

  UrlLoadParams params = UrlLoadParams::InNewTab(newTabURL);
  params.in_incognito = self.profile->IsOffTheRecord();
  params.append_to = OpenPosition::kSpecifiedIndex;
  params.insertion_index = webStateListIndex;
  self.URLLoader->Load(params);
}

- (void)insertItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  WebStateList* webStateList = self.webStateList;
  GridItemIdentifier* nextItemIdentifier;
  if (webStateList->ContainsIndex(nextWebStateIndex)) {
    const TabGroup* group =
        webStateList->GetGroupOfWebStateAt(nextWebStateIndex);
    if (group) {
      nextItemIdentifier = [GridItemIdentifier groupIdentifier:group
                                              withWebStateList:webStateList];
    } else {
      nextItemIdentifier = [GridItemIdentifier
          tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
    }
  }
  [self.consumer insertItem:item
                beforeItemID:nextItemIdentifier
      selectedItemIdentifier:[self activeIdentifier]];
}

- (void)moveItem:(GridItemIdentifier*)item
    beforeWebStateIndex:(int)nextWebStateIndex {
  GridItemIdentifier* nextItem = nil;
  if (self.webStateList->ContainsIndex(nextWebStateIndex)) {
    const TabGroup* group =
        self.webStateList->GetGroupOfWebStateAt(nextWebStateIndex);
    if (group) {
      nextItem = [GridItemIdentifier groupIdentifier:group
                                    withWebStateList:self.webStateList];
    } else {
      nextItem = [GridItemIdentifier
          tabIdentifier:self.webStateList->GetWebStateAt(nextWebStateIndex)];
    }
  }

  [self.consumer moveItem:item beforeItem:nextItem];
}

- (void)updateConsumerItemForWebState:(web::WebState*)webState {
  WebStateList* webStateList = self.webStateList;
  int index = webStateList->GetIndexOfWebState(webState);
  const TabGroup* group = nullptr;
  if (webStateList->ContainsIndex(index)) {
    group = webStateList->GetGroupOfWebStateAt(index);
  }
  GridItemIdentifier* item;
  if (group) {
    item = [GridItemIdentifier groupIdentifier:group
                              withWebStateList:webStateList];
  } else {
    item = [GridItemIdentifier tabIdentifier:webState];
  }
  [self.consumer replaceItem:item withReplacementItem:item];
}

- (void)closeTabGroup:(const TabGroup*)group andDeleteGroup:(BOOL)deleteGroup {
  if (!group) {
    return;
  }
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];

  WebStateList* groupWebStateList = [self groupWebStateList:group];
  if (!groupWebStateList) {
    // The group has already been removed.
    return;
  }

  if (groupWebStateList != _webStateList) {
    // `group` is not in the set of groups of the `_webStateList`, so `group`
    // should be a search result from a different window. Since this item is not
    // from the current browser, no UI updates will be sent to the current grid.
    // Notify the current grid consumer about the change.
    CHECK(_modeHolder.mode == TabGridMode::kSearch, base::NotFatalUntil::M130);
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier groupIdentifier:group
                           withWebStateList:groupWebStateList];
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:nil];
  }

  if (IsTabGroupSyncEnabled() && !deleteGroup) {
    [self showTabGroupSnackbarOrIPH:1];
    tab_groups::TabGroupSyncService* syncService =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(
            self.browser->GetProfile());
    tab_groups::utils::CloseTabGroupLocally(group, groupWebStateList,
                                            syncService);
  } else {
    // Using `CloseAllWebStatesInGroup` will result in calling the web state
    // list observers which will take care of updating the consumer.
    CloseAllWebStatesInGroup(*groupWebStateList, group,
                             WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)ungroupTabGroup:(const TabGroup*)group {
  if (!group) {
    return;
  }
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];

  WebStateList* groupWebStateList = [self groupWebStateList:group];
  if (!groupWebStateList) {
    // The group has already been removed.
    return;
  }

  if (groupWebStateList != _webStateList) {
    // `group` is not in the set of groups of the `_webStateList`, so `group`
    // should be a search result from a different window. Since this item is not
    // from the current browser, no UI updates will be sent to the current grid.
    // Notify the current grid consumer about the change.
    CHECK(_modeHolder.mode == TabGridMode::kSearch, base::NotFatalUntil::M130);
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier groupIdentifier:group
                           withWebStateList:groupWebStateList];
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:nil];
  }

  groupWebStateList->DeleteGroup(group);
}

- (BOOL)canHandleTabGroupDrop:(TabGroupInfo*)tabGroupInfo {
  return self.profile->IsOffTheRecord() == tabGroupInfo.incognito;
}

- (void)recordExternalURLDropped {
  base::UmaHistogramEnumeration(kUmaGridViewDragOrigin, DragItemOrigin::kOther);
}

- (void)showTabGroupSnackbarOrIPH:(int)closedGroups {
  if (!IsTabGroupSyncEnabled() || closedGroups < 1) {
    return;
  }
  [self.tabGroupsHandler
      showTabGridTabGroupSnackbarAfterClosingGroups:closedGroups];
  [self.tabGridToolbarHandler showSavedTabGroupIPH];
}

#pragma mark - WebStateListObserving

- (void)willChangeWebStateList:(WebStateList*)webStateList
                        change:(const WebStateListChangeDetach&)detachChange
                        status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    return;
  }

  // When the deleted tab is showing as an item (i.e. when it's not
  // grouped or shown as a search result), remove it from the grid.
  if (!detachChange.group() || _modeHolder.mode == TabGridMode::kSearch) {
    // Get the identifier to remove.
    web::WebState* detachedWebState = detachChange.detached_web_state();
    GridItemIdentifier* identifierToRemove =
        [GridItemIdentifier tabIdentifier:detachedWebState];

    // If the WebState is pinned and it is not in the consumer's items list,
    // consumer will filter it out in the method's implementation.
    [self.consumer removeItemWithIdentifier:identifierToRemove
                     selectedItemIdentifier:[self activeIdentifier]];
  }

  // The pinned WebState could be detached only in case it was displayed in
  // the Tab Search and was closed from the context menu. In such a case
  // there were no observation added for it. Therefore, there is no need to
  // remove one.
  if (![self isPinnedWebState:detachChange.detached_from_index()]) {
    [self removeObservationForWebState:detachChange.detached_web_state()];
  }
}

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(_webStateList, webStateList);
  if (webStateList->IsBatchInProgress()) {
    if (change.type() == WebStateListChange::Type::kInsert) {
      [self updateForTabInserted];
    }
    return;
  }

  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly: {
      const WebStateListChangeStatusOnly& selectionOnlyChange =
          change.As<WebStateListChangeStatusOnly>();
      if (selectionOnlyChange.pinned_state_changed()) {
        [self
            changePinnedStateForWebState:selectionOnlyChange.web_state()
                                 atIndex:selectionOnlyChange.index()
                            updatedGroup:selectionOnlyChange.old_group()
                                             ? selectionOnlyChange.old_group()
                                             : selectionOnlyChange.new_group()];
        break;
      }
      const TabGroup* oldGroup = selectionOnlyChange.old_group();
      const TabGroup* newGroup = selectionOnlyChange.new_group();

      if (oldGroup != newGroup) {
        // There is a change of group.
        if (oldGroup == nullptr) {
          // The tab was ungrouped and is moving to a group.
          web::WebState* currentWebState =
              _webStateList->GetWebStateAt(selectionOnlyChange.index());

          GridItemIdentifier* tabIdentifierToAddToGroup =
              [GridItemIdentifier tabIdentifier:currentWebState];

          [self.consumer removeItemWithIdentifier:tabIdentifierToAddToGroup
                           selectedItemIdentifier:[self activeIdentifier]];
        } else {
          // The tab left a group.
          GridItemIdentifier* oldGroupIdentifier =
              [GridItemIdentifier groupIdentifier:oldGroup
                                 withWebStateList:_webStateList];
          [self.consumer replaceItem:oldGroupIdentifier
                 withReplacementItem:oldGroupIdentifier];
        }

        if (newGroup) {
          // The tab joined a group.
          GridItemIdentifier* newGroupIdentifier =
              [GridItemIdentifier groupIdentifier:newGroup
                                 withWebStateList:_webStateList];

          [self.consumer replaceItem:newGroupIdentifier
                 withReplacementItem:newGroupIdentifier];
        } else {
          // The tab is now ungrouped.
          int webStateIndex = selectionOnlyChange.index();
          web::WebState* currentWebState =
              _webStateList->GetWebStateAt(webStateIndex);

          [self insertItem:[GridItemIdentifier tabIdentifier:currentWebState]
              beforeWebStateIndex:webStateIndex + 1];
        }

        // If the web state is the active one, the new group needs to be
        // highlighted.
        if (selectionOnlyChange.index() == webStateList->active_index()) {
          [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
        }
        break;
      }
      // The activation is handled after this switch statement.
      break;
    }
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detachChange =
          change.As<WebStateListChangeDetach>();
      if (detachChange.group()) {
        [self updateCellGroup:detachChange.group()];
      }
      // Do not manage other case scenarios as this is already handled in
      // `-willChangeWebStateList:change:status:` function.
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& moveChange =
          change.As<WebStateListChangeMove>();

      if (moveChange.pinned_state_changed()) {
        // The pinned state can be updated when a tab is moved.
        [self changePinnedStateForWebState:moveChange.moved_web_state()
                                   atIndex:moveChange.moved_to_index()
                              updatedGroup:moveChange.old_group()
                                               ? moveChange.old_group()
                                               : moveChange.new_group()];
      } else if (![self isPinnedWebState:moveChange.moved_to_index()]) {
        // BaseGridMediator handles only non pinned tabs because pinned tabs are
        // handled in PinnedTabsMediator.
        if (moveChange.old_group() == moveChange.new_group()) {
          // No group update.
          if (moveChange.old_group()) {
            // The cell moved inside its group, update the group.
            [self updateCellGroup:moveChange.old_group()];
          } else {
            // The cell is not in a group, move it.
            [self moveItem:[GridItemIdentifier
                               tabIdentifier:moveChange.moved_web_state()]
                beforeWebStateIndex:moveChange.moved_to_index() + 1];
          }
        } else {
          // The group has changed.
          if (moveChange.old_group()) {
            // The cell left a group.
            [self updateCellGroup:moveChange.old_group()];
          } else {
            // The cell wasn't in a group, remove it from the grid.
            [self.consumer removeItemWithIdentifier:
                               [GridItemIdentifier
                                   tabIdentifier:moveChange.moved_web_state()]
                             selectedItemIdentifier:[self activeIdentifier]];
          }
          if (moveChange.new_group()) {
            // The cell joined a group.
            [self updateCellGroup:moveChange.new_group()];
          } else {
            // The cell was removed from its group, add it to the grid.
            web::WebState* movedWebState = moveChange.moved_web_state();
            [self insertItem:[GridItemIdentifier tabIdentifier:movedWebState]
                beforeWebStateIndex:moveChange.moved_to_index() + 1];
          }
          // If the web state is the active one, the new group needs to be
          // highlighted.
          if (moveChange.moved_to_index() == webStateList->active_index()) {
            [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
          }
        }
      }
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replaceChange =
          change.As<WebStateListChangeReplace>();
      if ([self isPinnedWebState:replaceChange.index()]) {
        break;
      }
      web::WebState* replacedWebState = replaceChange.replaced_web_state();
      web::WebState* insertedWebState = replaceChange.inserted_web_state();
      [self.consumer replaceItem:[GridItemIdentifier
                                     tabIdentifier:replacedWebState]
             withReplacementItem:[GridItemIdentifier
                                     tabIdentifier:insertedWebState]];

      [self removeObservationForWebState:replacedWebState];
      [self addObservationForWebState:insertedWebState];
      break;
    }
    case WebStateListChange::Type::kInsert: {
      [self updateForTabInserted];
      const WebStateListChangeInsert& insertChange =
          change.As<WebStateListChangeInsert>();
      if ([self isPinnedWebState:insertChange.index()]) {
        [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
        break;
      }

      if (insertChange.group()) {
        [self updateCellGroup:insertChange.group()];
      } else {
        web::WebState* insertedWebState = insertChange.inserted_web_state();
        [self insertItem:[GridItemIdentifier tabIdentifier:insertedWebState]
            beforeWebStateIndex:insertChange.index() + 1];
      }
      [self addObservationForWebState:insertChange.inserted_web_state()];
      break;
    }
    case WebStateListChange::Type::kGroupCreate: {
      const WebStateListChangeGroupCreate& groupCreateChange =
          change.As<WebStateListChangeGroupCreate>();

      const TabGroup* currentGroup = groupCreateChange.created_group();
      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:currentGroup
                             withWebStateList:webStateList];
      CHECK(groupItemIdentifier.tabGroupItem.tabGroup);
      [self insertItem:groupItemIdentifier
          beforeWebStateIndex:groupItemIdentifier.tabGroupItem.tabGroup->range()
                                  .range_end() +
                              1];
      break;
    }
    case WebStateListChange::Type::kGroupVisualDataUpdate: {
      const WebStateListChangeGroupVisualDataUpdate& visualDataChange =
          change.As<WebStateListChangeGroupVisualDataUpdate>();
      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:visualDataChange.updated_group()
                             withWebStateList:webStateList];
      [self.consumer replaceItem:groupItemIdentifier
             withReplacementItem:groupItemIdentifier];

      break;
    }
    case WebStateListChange::Type::kGroupMove: {
      const WebStateListChangeGroupMove& groupMoveChange =
          change.As<WebStateListChangeGroupMove>();
      [self moveItem:[GridItemIdentifier
                          groupIdentifier:groupMoveChange.moved_group()
                         withWebStateList:webStateList]
          beforeWebStateIndex:groupMoveChange.moved_to_range().range_end()];
      break;
    }
    case WebStateListChange::Type::kGroupDelete: {
      const WebStateListChangeGroupDelete& groupDeleteChange =
          change.As<WebStateListChangeGroupDelete>();

      GridItemIdentifier* groupItemIdentifier =
          [GridItemIdentifier groupIdentifier:groupDeleteChange.deleted_group()
                             withWebStateList:_webStateList];
      [_selectedEditingItems removeItem:groupItemIdentifier];
      [self.consumer removeItemWithIdentifier:groupItemIdentifier
                       selectedItemIdentifier:[self activeIdentifier]];
      break;
    }
  }
  [self updateToolbarAfterNumberOfItemsChanged];
  if (status.active_web_state_change()) {
    [self.consumer selectItemWithIdentifier:[self activeIdentifier]];
  }
}

- (void)webStateListWillBeginBatchOperation:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  _scopedWebStateObservation->RemoveAllObservations();
}

- (void)webStateListBatchOperationEnded:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);

  // Clear selections.
  [_selectedEditingItems removeAllItems];

  [self addWebStateObservations];
  [self populateConsumerItems];
  [self updateToolbarAfterNumberOfItemsChanged];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDidStartLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

- (void)webStateDidChangeTitle:(web::WebState*)webState {
  [self updateConsumerItemForWebState:webState];
}

#pragma mark - TabGridModeObserving

- (void)tabGridModeDidChange:(TabGridModeHolder*)modeHolder {
  // Clear selections.
  [_selectedEditingItems removeAllItems];
  [self configureToolbarsButtons];
  [self.consumer setTabGridMode:modeHolder.mode];
}

#pragma mark - SnapshotStorageObserver

- (void)didUpdateSnapshotStorageWithSnapshotID:(SnapshotIDWrapper*)snapshotID {
  web::WebState* webState = nullptr;
  WebStateList* webStateList = self.webStateList;
  for (int i = webStateList->pinned_tabs_count(); i < webStateList->count();
       i++) {
    SnapshotTabHelper* snapshotTabHelper =
        SnapshotTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    if (snapshotID.snapshot_id == snapshotTabHelper->GetSnapshotID()) {
      webState = webStateList->GetWebStateAt(i);
      break;
    }
  }
  if (webState) {
    // It is possible to observe an updated snapshot for a WebState before
    // observing that the WebState has been added to the WebStateList. It is the
    // consumer's responsibility to ignore any updates before inserts.
    [self updateConsumerItemForWebState:webState];
  }
}

#pragma mark - GridCommands

- (BOOL)addNewItem {
  // The incognito mediator's Browser is briefly set to nil after the last
  // incognito tab is closed.
  if (!self.browser || !self.profile) {
    return NO;
  }

  int webStateListCount = self.webStateList->count();

  // The function is clamping the value, so it safe to pass the total count of
  // the WebState even if it is supposed to be a grid index.
  [self insertNewWebStateAtGridIndex:webStateListCount
                             withURL:GURL(kChromeUINewTabURL)];
  return webStateListCount != self.webStateList->count();
}

- (void)selectItemWithID:(web::WebStateID)itemID
                    pinned:(BOOL)pinned
    isFirstActionOnTabGrid:(BOOL)isFirstActionOnTabGrid {
  WebStateSearchCriteria searchCriteria{
      .identifier = itemID,
      .pinned_state = pinned ? PinnedState::kPinned : PinnedState::kNonPinned,
  };

  int index = GetWebStateIndex(self.webStateList, searchCriteria);
  WebStateList* itemWebStateList = self.webStateList;
  if (index == WebStateList::kInvalidIndex) {
    if (pinned) {
      return;
    }
    // If this is a search result, it may contain items from other windows or
    // from the inactive browser - check inactive browser and other windows
    // before giving up.
    BrowserList* browserList = BrowserListFactory::GetForProfile(self.profile);
    Browser* browser = GetBrowserForNonPinnedTabWithId(
        browserList, itemID, self.profile->IsOffTheRecord());

    if (!browser) {
      return;
    }

    if (browser->IsInactive()) {
      base::RecordAction(
          base::UserMetricsAction("MobileTabGridOpenInactiveTabSearchResult"));
      index = itemWebStateList->count();
      MoveTabToBrowser(itemID, self.browser, index);
    } else {
      // Other windows case.
      itemWebStateList = browser->GetWebStateList();
      index = GetWebStateIndex(itemWebStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
      SceneState* targetSceneState = browser->GetSceneState();
      SceneState* currentSceneState = self.browser->GetSceneState();

      UISceneActivationRequestOptions* options =
          [[UISceneActivationRequestOptions alloc] init];
      options.requestingScene = currentSceneState.scene;

      [[UIApplication sharedApplication]
          requestSceneSessionActivation:targetSceneState.scene.session
                           userActivity:nil
                                options:options
                           errorHandler:^(NSError* error) {
                             LOG(ERROR) << base::SysNSStringToUTF8(
                                 error.localizedDescription);
                             NOTREACHED_IN_MIGRATION();
                           }];
    }
  }

  web::WebState* selectedWebState = itemWebStateList->GetWebStateAt(index);
  LogPriceDropMetrics(selectedWebState);

  base::TimeDelta timeSinceLastActivation =
      base::Time::Now() - selectedWebState->GetLastActiveTime();
  base::UmaHistogramCustomTimes(
      "IOS.TabGrid.TabSelected.TimeSinceLastActivation",
      timeSinceLastActivation, base::Minutes(1), base::Days(24), 50);

  // Don't attempt a no-op activation. Normally this is not an issue, but it's
  // possible that this method (-selectItemWithID:) is being called as part of
  // a WebStateListObserver callback, in which case even a no-op activation
  // will cause a CHECK().
  if (selectedWebState == itemWebStateList->GetActiveWebState()) {
    // In search mode, the consumer doesn't have any information about the
    // selected item. So even if the active WebState is the same as the one that
    // is being selected, make sure that the consumer updates its selected item.
    [self.consumer
        selectItemWithIdentifier:[GridItemIdentifier
                                     tabIdentifier:selectedWebState]];
    return;
  } else {
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridMoveToExistingTab"));
    if (isFirstActionOnTabGrid) {
      int activeWebStateIndex = itemWebStateList->active_index();
      BOOL adjacentTabSelected =
          std::abs(index - activeWebStateIndex) == 1 &&
          index != WebStateList::kInvalidIndex &&
          activeWebStateIndex != WebStateList::kInvalidIndex;
      if (adjacentTabSelected && self.browser) {
        TabBasedIPHBrowserAgent* tabBasedIPHBrowserAgent =
            TabBasedIPHBrowserAgent::FromBrowser(self.browser);
        tabBasedIPHBrowserAgent->NotifySwitchToAdjacentTabFromTabGrid();
      }
    }
  }

  // Avoid a reentrant activation. This is a fix for crbug.com/1134663, although
  // ignoring the selection at this point may do weird things.
  if (itemWebStateList->IsMutating()) {
    return;
  }

  // It should be safe to activate here.
  itemWebStateList->ActivateWebStateAt(index);
}

- (void)selectTabGroup:(const TabGroup*)tabGroup {
  WebStateList* webStateList = self.webStateList;

  if (webStateList->ContainsGroup(tabGroup)) {
    [self.tabGroupsHandler showTabGroup:tabGroup];
    return;
  }

  BOOL incognito = self.profile->IsOffTheRecord();
  // If this is a search result, it may contain items from other windows or
  // from the inactive browser - check other windows before giving up.
  BrowserList* browserList = BrowserListFactory::GetForProfile(self.profile);
  Browser* browser = GetBrowserForGroup(browserList, tabGroup, incognito);

  if (!browser) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridOpenSearchResultInAnotherWindow"));

  SceneState* targetSceneState = browser->GetSceneState();
  SceneState* currentSceneState = self.browser->GetSceneState();

  UISceneActivationRequestOptions* options =
      [[UISceneActivationRequestOptions alloc] init];
  options.requestingScene = currentSceneState.scene;

  [[UIApplication sharedApplication]
      requestSceneSessionActivation:targetSceneState.scene.session
                       userActivity:nil
                            options:options
                       errorHandler:^(NSError* error) {
                         LOG(ERROR) << base::SysNSStringToUTF8(
                             error.localizedDescription);
                         NOTREACHED_IN_MIGRATION();
                       }];

  if (!targetSceneState.UIEnabled) {
    return;
  }

  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  TabGridOpeningMode openingMode =
      incognito ? TabGridOpeningMode::kIncognito : TabGridOpeningMode::kRegular;
  [applicationHandler displayTabGridInMode:openingMode];

  id<TabGroupsCommands> tabGroupsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), TabGroupsCommands);
  [tabGroupsHandler showTabGroup:tabGroup];
}

- (BOOL)isItemWithIDSelected:(web::WebStateID)itemID {
  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{.identifier = itemID});
  if (index == WebStateList::kInvalidIndex) {
    return NO;
  }
  return index == self.webStateList->active_index();
}

- (void)setPinState:(BOOL)pinState forItemWithID:(web::WebStateID)itemID {
  SetWebStatePinnedState(self.webStateList, itemID, pinState);
}

- (void)closeItemWithID:(web::WebStateID)itemID {
  [self.tabGridIdleStatusHandler
      tabGridDidPerformAction:TabGridActionType::kInPageAction];
  int index = GetWebStateIndex(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                               });
  if (index != WebStateList::kInvalidIndex) {
    self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
    return;
  }

  TabSwitcherItem* itemToRemove =
      [[TabSwitcherItem alloc] initWithIdentifier:itemID];

  GridItemIdentifier* identifierToRemove =
      [[GridItemIdentifier alloc] initWithTabItem:itemToRemove];

  // `index` is `WebStateList::kInvalidIndex`, so `itemID` should be a search
  // result from a different window. Since this item is not from the current
  // browser, no UI updates will be sent to the current grid. Notify the current
  // grid consumer about the change.
  [self.consumer removeItemWithIdentifier:identifierToRemove
                   selectedItemIdentifier:nil];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSearchCloseTabFromAnotherWindow"));

  BrowserList* browserList = BrowserListFactory::GetForProfile(self.profile);
  Browser* browser = GetBrowserForNonPinnedTabWithId(
      browserList, itemID, self.profile->IsOffTheRecord());

  // If this tab is still associated with another browser, remove it from the
  // associated web state list.
  if (browser) {
    WebStateList* itemWebStateList = browser->GetWebStateList();
    index = GetWebStateIndex(itemWebStateList,
                             WebStateSearchCriteria{
                                 .identifier = itemID,
                                 .pinned_state = PinnedState::kNonPinned,
                             });
    itemWebStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
  }
}

- (void)closeItemsWithTabIDs:(const std::set<web::WebStateID>&)tabIDs
                    groupIDs:(const std::set<tab_groups::TabGroupId>&)groupIDs
                    tabCount:(int)tabCount {
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.CloseTabs", tabCount);
  RecordTabGridCloseTabsCount(tabCount);

  WebStateList* webStateList = self.webStateList;
  int closedGroupsCount = groupIDs.size();

  if (closedGroupsCount > 0) {
    tab_groups::TabGroupSyncService* syncService = nil;
    if (IsTabGroupSyncEnabled()) {
      syncService = tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          self.browser->GetProfile());
    }

    // Find and close all groups in `groupIDs`.
    for (const TabGroup* group : webStateList->GetGroups()) {
      tab_groups::TabGroupId groupID = group->tab_group_id();
      if (groupIDs.contains(groupID)) {
        // CloseTabGroupLocally handles it correctly when syncService is nil.
        tab_groups::utils::CloseTabGroupLocally(group, webStateList,
                                                syncService);
      }
    }
  }

  {
    WebStateList::ScopedBatchOperation lock =
        webStateList->StartBatchOperation();
    for (const web::WebStateID itemID : tabIDs) {
      const int index = GetWebStateIndex(
          webStateList,
          WebStateSearchCriteria{.identifier = itemID,
                                 .pinned_state = PinnedState::kNonPinned});
      if (index != WebStateList::kInvalidIndex) {
        GridItemIdentifier* identifierToRemove = [GridItemIdentifier
            tabIdentifier:webStateList->GetWebStateAt(index)];
        [_selectedEditingItems removeItem:identifierToRemove];
        webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
      }
    }
  }

  const bool allTabsClosed = webStateList->empty();
  if (allTabsClosed) {
    if (!self.profile->IsOffTheRecord()) {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllRegularTabsConfirmed"));
    } else {
      base::RecordAction(base::UserMetricsAction(
          "MobileTabGridSelectionCloseAllIncognitoTabsConfirmed"));
    }
  }

  if (IsTabGroupSyncEnabled() && closedGroupsCount > 0) {
    [self showTabGroupSnackbarOrIPH:closedGroupsCount];
  }
}

- (void)deleteTabGroup:(base::WeakPtr<const TabGroup>)group
            sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    [self.tabGroupsHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kDeleteTabGroup
                                    group:group
                               sourceView:sourceView];
    return;
  }

  DCHECK(!IsTabGroupSyncEnabled());
  [self closeTabGroup:group.get() andDeleteGroup:YES];
}

- (void)closeTabGroup:(base::WeakPtr<const TabGroup>)group {
  [self closeTabGroup:group.get() andDeleteGroup:NO];
}

- (void)ungroupTabGroup:(base::WeakPtr<const TabGroup>)group
             sourceView:(UIView*)sourceView {
  if (IsTabGroupSyncEnabled()) {
    [self.tabGroupsHandler
        showTabGroupConfirmationForAction:TabGroupActionType::kUngroupTabGroup
                                    group:group
                               sourceView:sourceView];
    return;
  }

  DCHECK(!IsTabGroupSyncEnabled());
  [self ungroupTabGroup:group.get()];
}

- (void)closeAllItems {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)saveAndCloseAllItems {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)undoCloseAllItems {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)discardSavedClosedItems {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)searchItemsWithText:(NSString*)searchText {
  TabsSearchService* searchService =
      TabsSearchServiceFactory::GetForProfile(self.profile);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  searchService->Search(
      searchTerm,
      base::BindOnce(^(
          std::vector<TabsSearchService::TabsSearchBrowserResults> results) {
        NSMutableArray* currentBrowserItems = [[NSMutableArray alloc] init];
        NSMutableArray* remainingItems = [[NSMutableArray alloc] init];
        for (const TabsSearchService::TabsSearchBrowserResults& browserResults :
             results) {
          if (IsTabGroupInGridEnabled()) {
            for (const TabGroup* group : browserResults.tab_groups) {
              GridItemIdentifier* item = [GridItemIdentifier
                   groupIdentifier:group
                  withWebStateList:browserResults.browser->GetWebStateList()];
              if (browserResults.browser == self.browser) {
                [currentBrowserItems addObject:item];
              } else {
                [remainingItems addObject:item];
              }
            }
          }

          for (web::WebState* webState : browserResults.web_states) {
            GridItemIdentifier* item =
                [GridItemIdentifier tabIdentifier:webState];

            if (browserResults.browser == self.browser) {
              [currentBrowserItems addObject:item];
            } else {
              [remainingItems addObject:item];
            }
          }
        }

        NSArray* allItems = nil;
        // If there are results from Browsers other than the current one,
        // append those results to the end.
        if (remainingItems.count) {
          allItems = [currentBrowserItems
              arrayByAddingObjectsFromArray:remainingItems];
        } else {
          allItems = currentBrowserItems;
        }

        [self.consumer populateItems:allItems selectedItemIdentifier:nil];
      }));
}

- (void)resetToAllItems {
  [self populateConsumerItems];
}

#pragma mark - SuggestedActionsDelegate

- (void)fetchSearchHistoryResultsCountForText:(NSString*)searchText
                                   completion:(void (^)(size_t))completion {
  CHECK(!self.profile->IsOffTheRecord());
  TabsSearchService* search_service =
      TabsSearchServiceFactory::GetForProfile(self.profile);
  const std::u16string& searchTerm = base::SysNSStringToUTF16(searchText);
  search_service->SearchHistory(searchTerm,
                                base::BindOnce(^(size_t resultCount) {
                                  completion(resultCount);
                                }));
}

#pragma mark - TabCollectionDragDropHandler

- (NSArray<UIDragItem*>*)allSelectedDragItems {
  NSMutableArray<UIDragItem*>* dragItems = [[NSMutableArray alloc] init];
  for (GridItemIdentifier* itemID in _selectedEditingItems.itemsIdentifiers) {
    switch (itemID.type) {
      case GridItemType::kInactiveTabsButton:
        // Inactive Tabs button is not draggable and not stored in
        // `_selectedEditingItems`.
        NOTREACHED();
      case GridItemType::kTab: {
        UIDragItem* dragItem =
            [self dragItemForItemWithID:itemID.tabSwitcherItem.identifier];
        if (dragItem) {
          [dragItems addObject:dragItem];
        }
        break;
      }
      case GridItemType::kGroup: {
        UIDragItem* dragItem =
            [self dragItemForTabGroupItem:itemID.tabGroupItem];
        if (dragItem) {
          [dragItems addObject:dragItem];
        }
        break;
      }
      case GridItemType::kSuggestedActions:
        // Suggested actions items are not dragable and not stored in
        // `_selectedEditingItems`.
        NOTREACHED();
    }
  }
  return dragItems;
}

- (UIDragItem*)dragItemForTabGroupItem:(TabGroupItem*)tabGroupItem {
  return CreateTabGroupDragItem(tabGroupItem.tabGroup, self.profile);
}

- (UIDragItem*)dragItemForItem:(TabSwitcherItem*)item {
  return [self dragItemForItemWithID:item.identifier];
}

- (void)dragSessionDidEnd {
  // Update buttons as the number of items or the number of selected items might
  // have changed.
  [self.toolbarsMutator setButtonsEnabled:YES];
  [self configureToolbarsButtons];
}

- (UIDropOperation)dropOperationForDropSession:(id<UIDropSession>)session
                                       toIndex:(NSUInteger)destinationIndex {
  UIDragItem* dragItem = session.localDragSession.items.firstObject;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);
    if (tabInfo.profile != self.profile) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }

    if (self.profile->IsOffTheRecord() == tabInfo.incognito) {
      return UIDropOperationMove;
    }

    // Tabs of different profiles (regular/incognito) cannot be dropped.
    return UIDropOperationForbidden;
  }
  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        static_cast<TabGroupInfo*>(dragItem.localObject);
    if (tabGroupInfo.profile != self.profile) {
      // Tabs from different profiles cannot be dropped.
      return UIDropOperationForbidden;
    }
    return [self canHandleTabGroupDrop:tabGroupInfo] ? UIDropOperationMove
                                                     : UIDropOperationForbidden;
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
  WebStateList* webStateList = self.webStateList;

  // Tab move operations only originate from Chrome so a local object is used.
  // Local objects allow synchronous drops, whereas NSItemProvider only allows
  // asynchronous drops.
  if ([dragItem.localObject isKindOfClass:[TabInfo class]]) {
    TabInfo* tabInfo = static_cast<TabInfo*>(dragItem.localObject);

    if (IsPinnedTabsEnabled()) {
      // Try to unpin the tab, if not pinned nothing happens.
      SetWebStatePinnedState(webStateList, tabInfo.tabID,
                             /*pin_state=*/false);
    }

    int sourceWebStateIndex =
        GetWebStateIndex(webStateList, WebStateSearchCriteria{
                                           .identifier = tabInfo.tabID,
                                       });

    if (sourceWebStateIndex == WebStateList::kInvalidIndex) {
      // Move tab across Browsers.
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
      int destinationWebStateIndex =
          WebStateIndexFromGridDropItemIndex(webStateList, destinationIndex);

      MoveTabToBrowser(tabInfo.tabID, self.browser, destinationWebStateIndex);
      return;
    }

    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
    } else {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameBrowser);
    }

    // Reorder tabs.
    int destinationWebStateIndex = WebStateIndexFromGridDropItemIndex(
        webStateList, destinationIndex, sourceWebStateIndex);
    const auto insertionParams =
        WebStateList::InsertionParams::AtIndex(destinationWebStateIndex);
    MoveWebStateWithIdentifierToInsertionParams(
        tabInfo.tabID, insertionParams, webStateList, fromSameCollection);
    return;
  }

  if ([dragItem.localObject isKindOfClass:[TabGroupInfo class]]) {
    TabGroupInfo* tabGroupInfo =
        static_cast<TabGroupInfo*>(dragItem.localObject);
    // Early return if the group has been closed during the drag an drop.
    if (!tabGroupInfo.tabGroup) {
      return;
    }
    if (fromSameCollection) {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kSameCollection);
      CHECK(tabGroupInfo.tabGroup);
      int sourceIndex = tabGroupInfo.tabGroup->range().range_begin();
      int nextWebStateIndex = WebStateIndexAfterGridDropItemIndex(
          webStateList, destinationIndex, sourceIndex);
      webStateList->MoveGroup(tabGroupInfo.tabGroup, nextWebStateIndex);
      return;
    } else {
      base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                    DragItemOrigin::kOtherBrowser);
    }

    int destinationWebStateIndex =
        WebStateIndexAfterGridDropItemIndex(webStateList, destinationIndex);
    tab_groups::utils::MoveTabGroupToBrowser(
        tabGroupInfo.tabGroup, self.browser, destinationWebStateIndex);
    return;
  }

  // Handle URLs from within Chrome synchronously using a local object.
  if ([dragItem.localObject isKindOfClass:[URLInfo class]]) {
    URLInfo* droppedURL = static_cast<URLInfo*>(dragItem.localObject);
    [self insertNewWebStateAtGridIndex:destinationIndex withURL:droppedURL.URL];
    base::UmaHistogramEnumeration(kUmaGridViewDragOrigin,
                                  DragItemOrigin::kOther);
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

  [self recordExternalURLDropped];

  __weak BaseGridMediator* weakSelf = self;
  auto loadHandler =
      ^(__kindof id<NSItemProviderReading> providedItem, NSError* error) {
        dispatch_async(dispatch_get_main_queue(), ^{
          [placeholderContext deletePlaceholder];
          NSURL* droppedURL = static_cast<NSURL*>(providedItem);
          [weakSelf
              insertNewWebStateAtGridIndex:destinationIndex
                                   withURL:net::GURLWithNSURL(droppedURL)];
        });
      };
  [itemProvider loadObjectOfClass:[NSURL class] completionHandler:loadHandler];
}

#pragma mark - Private

// Returns a SnapshotStorageWrapper for the current browser.
- (SnapshotStorageWrapper*)snapshotStorage {
  if (!self.browser) {
    return nil;
  }
  return SnapshotBrowserAgent::FromBrowser(self.browser)->snapshot_storage();
}

- (void)addItemsWithIDsToReadingList:(const std::set<web::WebStateID>&)itemIDs {
  [self.delegate dismissPopovers];

  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToReadingList",
                              itemIDs.size());

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:itemIDs];

  ReadingListAddCommand* command =
      [[ReadingListAddCommand alloc] initWithURLs:URLs];
  ReadingListBrowserAgent* readingListBrowserAgent =
      ReadingListBrowserAgent::FromBrowser(self.browser);
  readingListBrowserAgent->AddURLsToReadingList(command.URLs);
}

- (void)addItemsWithIDsToBookmarks:(const std::set<web::WebStateID>&)itemIDs {
  id<BookmarksCommands> bookmarkHandler =
      HandlerForProtocol(_browser->GetCommandDispatcher(), BookmarksCommands);

  if (!bookmarkHandler) {
    return;
  }
  [self.delegate dismissPopovers];
  base::RecordAction(
      base::UserMetricsAction("MobileTabGridAddedMultipleNewBookmarks"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.AddToBookmarks",
                              itemIDs.size());

  NSArray<URLWithTitle*>* URLs = [self urlsWithTitleFromItemIDs:itemIDs];

  [bookmarkHandler bookmarkWithFolderChooser:URLs];
}

- (NSArray<URLWithTitle*>*)urlsWithTitleFromItemIDs:
    (const std::set<web::WebStateID>&)itemIDs {
  NSMutableArray<URLWithTitle*>* URLs = [[NSMutableArray alloc] init];
  for (const web::WebStateID itemID : itemIDs) {
    TabItem* item = GetTabItem(self.webStateList,
                               WebStateSearchCriteria{
                                   .identifier = itemID,
                                   .pinned_state = PinnedState::kNonPinned,
                               });
    URLWithTitle* URL = [[URLWithTitle alloc] initWithURL:item.URL
                                                    title:item.title];
    [URLs addObject:URL];
  }
  return URLs;
}

// Inserts/removes a non pinned item to/from the collection.
- (void)changePinnedStateForWebState:(web::WebState*)webState
                             atIndex:(int)index
                        updatedGroup:(const TabGroup*)group {
  if ([self isPinnedWebState:index]) {
    if (group) {
      [self updateCellGroup:group];
    } else {
      GridItemIdentifier* identifierToRemove =
          [GridItemIdentifier tabIdentifier:webState];
      [self.consumer removeItemWithIdentifier:identifierToRemove
                       selectedItemIdentifier:[self activeIdentifier]];
    }
    [self removeObservationForWebState:webState];
  } else {
    if (group) {
      [self updateCellGroup:group];
    } else {
      [self insertItem:[GridItemIdentifier tabIdentifier:webState]
          beforeWebStateIndex:index + 1];
    }
    [self addObservationForWebState:webState];
  }
}

- (BOOL)isPinnedWebState:(int)index {
  if (IsPinnedTabsEnabled() && self.webStateList->IsWebStatePinnedAt(index)) {
    return YES;
  }
  return NO;
}

// Updates toolbars when the number of web state might be changed.
- (void)updateToolbarAfterNumberOfItemsChanged {
  if (_modeHolder.mode == TabGridMode::kSelection &&
      self.webStateList->empty()) {
    // Exit selection mode if there are no more tabs.
    _modeHolder.mode = TabGridMode::kNormal;
  } else {
    // Update toolbar's buttons as the number of tabs have probably changed so
    // the options changed (ex: "Undo" may be available now).
    [self configureToolbarsButtons];
  }
}

// Returns a drag item for the given `itemID`.
- (UIDragItem*)dragItemForItemWithID:(web::WebStateID)itemID {
  web::WebState* webState = GetWebState(
      self.webStateList, WebStateSearchCriteria{
                             .identifier = itemID,
                             .pinned_state = PinnedState::kNonPinned,
                         });
  return CreateTabDragItem(webState);
}

// Returns the menu to display when the Add To button is selected for `items`.
- (NSArray<UIMenuElement*>*)addToButtonMenuElements {
  if (!self.browser) {
    return nil;
  }

  NSMutableArray<UIMenuElement*>* actions = [[NSMutableArray alloc] init];

  ActionFactory* actionFactory = [[ActionFactory alloc]
      initWithScenario:kMenuScenarioHistogramTabGridAddTo];

  __weak BaseGridMediator* weakSelf = self;

  if (IsTabGroupInGridEnabled()) {
    auto addToGroupBlock = ^(const TabGroup* group) {
      [weakSelf addSelectedElementsToGroup:group];
    };
    UIMenuElement* addToGroup = [actionFactory
        menuToAddTabToGroupWithGroups:GetAllGroupsForProfile(_profile)
                         numberOfTabs:_selectedEditingItems.tabsCount
                                block:addToGroupBlock];
    [actions addObject:[UIMenu menuWithTitle:@""
                                       image:nil
                                  identifier:nil
                                     options:UIMenuOptionsDisplayInline
                                    children:@[ addToGroup ]]];
  }

  // Copy the set of items, so that the following block can use it.
  std::set<web::WebStateID> shareableTabsCopy =
      [_selectedEditingItems sharableTabs];

  UIAction* addToReadingListAction =
      [actionFactory actionToAddToReadingListWithBlock:^{
        [weakSelf addItemsWithIDsToReadingList:shareableTabsCopy];
      }];

  UIAction* bookmarkAction = [actionFactory actionToBookmarkWithBlock:^{
    [weakSelf addItemsWithIDsToBookmarks:shareableTabsCopy];
  }];
  // Bookmarking can be disabled from prefs (from an enterprise policy),
  // if that's the case grey out the option in the menu.
  BOOL isEditBookmarksEnabled =
      self.browser->GetProfile()->GetPrefs()->GetBoolean(
          bookmarks::prefs::kEditBookmarksEnabled);
  if (!isEditBookmarksEnabled) {
    bookmarkAction.attributes = UIMenuElementAttributesDisabled;
  }
  if (shareableTabsCopy.size() == 0) {
    addToReadingListAction.attributes = UIMenuElementAttributesDisabled;
    bookmarkAction.attributes = UIMenuElementAttributesDisabled;
  }

  [actions addObject:addToReadingListAction];
  [actions addObject:bookmarkAction];

  return actions;
}

// Adds all the current selected elements to `group`. Pass nullptr to add to a
// new group.
- (void)addSelectedElementsToGroup:(const TabGroup*)group {
  std::set<web::WebStateID> selectedTabs = [_selectedEditingItems allTabs];
  if (group == nullptr) {
    [self.tabGroupsHandler showTabGroupCreationForTabs:selectedTabs];
  } else {
    WebStateList::ScopedBatchOperation lock =
        self.webStateList->StartBatchOperation();
    for (web::WebStateID webStateID : selectedTabs) {
      MoveTabToGroup(webStateID, group, _profile);
    }
  }
}

// Returns the associated WebStateList for the given `group`.
- (WebStateList*)groupWebStateList:(const TabGroup*)group {
  if (_webStateList->ContainsGroup(group)) {
    return _webStateList;
  }
  BrowserList* browserList = BrowserListFactory::GetForProfile(self.profile);
  Browser* browser =
      GetBrowserForGroup(browserList, group, self.profile->IsOffTheRecord());
  if (!browser) {
    return nullptr;
  }
  return browser->GetWebStateList();
}

// Updates the cell of the given `group`.
- (void)updateCellGroup:(const TabGroup*)group {
  GridItemIdentifier* groupIdentifier =
      [GridItemIdentifier groupIdentifier:group
                         withWebStateList:self.webStateList];
  [self.consumer replaceItem:groupIdentifier
         withReplacementItem:groupIdentifier];
}

#pragma mark - TabGridPageMutator

- (void)currentlySelectedGrid:(BOOL)selected {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)setPageAsActive {
  NOTREACHED() << "Should be implemented in a subclass.";
}

#pragma mark - TabGridToolbarsGridDelegate

- (void)closeAllButtonTapped:(id)sender {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)doneButtonTapped:(id)sender {
  // Tapping Done when in selection mode, should only return back to the normal
  // mode.
  if (_modeHolder.mode == TabGridMode::kSelection) {
    _modeHolder.mode = TabGridMode::kNormal;
    // Records action when user exit the selection mode.
    base::RecordAction(base::UserMetricsAction("MobileTabGridSelectionDone"));
  } else {
    base::RecordAction(base::UserMetricsAction("MobileTabGridDone"));
    [self.tabGridHandler exitTabGrid];
  }
}

- (void)newTabButtonTapped:(id)sender {
  NOTREACHED() << "Should be implemented in a subclass.";
}

- (void)selectAllButtonTapped:(id)sender {
  NSUInteger selectedItemsCount = _selectedEditingItems.tabsCount;
  BOOL allItemsSelected =
      static_cast<int>(selectedItemsCount) ==
      (self.webStateList->count() - self.webStateList->pinned_tabs_count());

  // Deselect all items if they are all already selected.
  if (allItemsSelected) {
    [_selectedEditingItems removeAllItems];
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionDeselectAll"));
  } else {
    NSArray<GridItemIdentifier*>* identifiers = CreateItems(self.webStateList);
    for (GridItemIdentifier* identifier in identifiers) {
      [self addToSelectionItemID:identifier];
    }
    base::RecordAction(
        base::UserMetricsAction("MobileTabGridSelectionSelectAll"));
  }
  [self.consumer reload];
  [self configureToolbarsButtons];
}

- (void)searchButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridSearchTabs"));
  _modeHolder.mode = TabGridMode::kSearch;
}

- (void)cancelSearchButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridCancelSearchTabs"));
  _modeHolder.mode = TabGridMode::kNormal;
}

- (void)closeSelectedTabs:(id)sender {
  [self.delegate dismissPopovers];

  std::set<web::WebStateID> selectedTabIDs;
  std::set<tab_groups::TabGroupId> selectedGroupIDs;
  int tabCount = 0;

  for (GridItemIdentifier* identifier in _selectedEditingItems
           .itemsIdentifiers) {
    switch (identifier.type) {
      case GridItemType::kInactiveTabsButton:
        NOTREACHED();
      case GridItemType::kTab: {
        selectedTabIDs.insert(identifier.tabSwitcherItem.identifier);
        tabCount++;
        break;
      }
      case GridItemType::kGroup: {
        CHECK(identifier.tabGroupItem.tabGroup);
        const TabGroup* group = identifier.tabGroupItem.tabGroup;
        selectedGroupIDs.insert(group->tab_group_id());
        tabCount += group->range().count();
        break;
      }
      case GridItemType::kSuggestedActions:
        NOTREACHED();
    }
  }

  [self.delegate baseGridMediator:self
      showCloseConfirmationWithTabIDs:selectedTabIDs
                             groupIDs:selectedGroupIDs
                             tabCount:tabCount
                               anchor:sender];
}

- (void)shareSelectedTabs:(id)sender {
  [self.delegate dismissPopovers];

  base::RecordAction(
      base::UserMetricsAction("MobileTabGridSelectionShareTabs"));
  base::UmaHistogramCounts100("IOS.TabGrid.Selection.ShareTabs",
                              _selectedEditingItems.sharableTabsCount);
  [self.delegate baseGridMediator:self
                        shareURLs:[_selectedEditingItems selectedTabsURLs]
                           anchor:sender];
}

- (void)selectTabsButtonTapped:(id)sender {
  base::RecordAction(base::UserMetricsAction("MobileTabGridSelectTabs"));
  _modeHolder.mode = TabGridMode::kSelection;
}

#pragma mark - GridViewControllerMutator

- (void)userTappedOnItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kInactiveTabsButton ||
        itemID.type == GridItemType::kGroup ||
        itemID.type == GridItemType::kTab);
  if (_modeHolder.mode == TabGridMode::kSelection) {
    CHECK(itemID.type != GridItemType::kInactiveTabsButton);
    if ([self isItemSelected:itemID]) {
      [self removeFromSelectionItemID:itemID];
    } else {
      [self addToSelectionItemID:itemID];
    }
  }
}

- (void)addToSelectionItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kTab ||
        itemID.type == GridItemType::kGroup);
  if (_modeHolder.mode != TabGridMode::kSelection) {
    base::debug::DumpWithoutCrashing();
    return;
  }
  [_selectedEditingItems addItem:itemID];
  [self configureToolbarsButtons];
}

- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID {
  CHECK(itemID.type == GridItemType::kTab ||
        itemID.type == GridItemType::kGroup);
  if (_modeHolder.mode != TabGridMode::kSelection) {
    return;
  }

  [_selectedEditingItems removeItem:itemID];
  [self configureToolbarsButtons];
}

- (void)closeItemWithIdentifier:(GridItemIdentifier*)identifier {
  switch (identifier.type) {
    case GridItemType::kInactiveTabsButton:
      NOTREACHED();
    case GridItemType::kTab:
      [self closeItemWithID:identifier.tabSwitcherItem.identifier];
      break;
    case GridItemType::kGroup: {
      const TabGroup* group = identifier.tabGroupItem.tabGroup;
      [self closeTabGroup:group andDeleteGroup:NO];
      break;
    }
    case GridItemType::kSuggestedActions:
      NOTREACHED();
  }
}

#pragma mark - BaseGridMediatorItemProvider

- (BOOL)isItemSelected:(GridItemIdentifier*)itemID {
  return [_selectedEditingItems containItem:itemID];
}

@end

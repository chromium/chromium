// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_mediator.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_utils.h"
#import "ios/chrome/browser/shared/model/utils/web_state_deferred_executor.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_logger.h"
#import "ios/chrome/browser/tab_picker/coordinator/tab_picker_snackbar_presenter.h"
#import "ios/chrome/browser/tab_picker/ui/tab_picker_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/public/web_state.h"

@interface TabPickerMediator () <WebStateDeferredExecutorDelegate>
@end

@implementation TabPickerMediator {
  /// The grid consumer.
  __weak id<TabCollectionConsumer> _gridConsumer;
  /// The tab picker consumer.
  __weak id<TabPickerConsumer> _tabPickerConsumer;
  /// The delegate for tabs attachment.
  __weak id<TabsAttachmentDelegate> _tabsAttachmentDelegate;
  /// Stores the unique identifiers of web states that have valid cached APC
  /// (Annotated Page Content) data.
  std::set<std::string> _validAPCwebStatesIDs;
  /// Stores the unique identifiers of web states that have failed to load.
  NSMutableSet<GridItemIdentifier*>* _failedLoadedItemIDs;
  /// Utility to delay execution of blocks until a web state is loaded.
  WebStateDeferredExecutor* _webStateDeferredExecutor;
}

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
                   tabPickerConsumer:(id<TabPickerConsumer>)tabPickerConsumer
              tabsAttachmentDelegate:
                  (id<TabsAttachmentDelegate>)tabsAttachmentDelegate {
  TabGridModeHolder* modeHolder =
      [[TabGridModeHolder alloc] initWithTabGridState:nil];
  modeHolder.mode = TabGridMode::kSelection;
  self = [super initWithModeHolder:modeHolder];

  if (self) {
    _gridConsumer = gridConsumer;
    _tabPickerConsumer = tabPickerConsumer;
    _tabsAttachmentDelegate = tabsAttachmentDelegate;

    [_tabPickerConsumer
        setSelectedTabsCount:self.selectedEditingItems.tabsCount];

    _webStateDeferredExecutor = [[WebStateDeferredExecutor alloc] init];
    _webStateDeferredExecutor.delegate = self;
    _failedLoadedItemIDs = [[NSMutableSet alloc] init];
  }

  return self;
}

- (void)setBrowser:(Browser*)browser {
  [super setBrowser:browser];

  if (!self.webStateList) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self createGridItemsWithCompletion:^(NSArray<GridItemIdentifier*>* items) {
    [weakSelf populateGridItems:items];
  }];

  [self fetchPageContexts];
}

- (id<TabCollectionConsumer>)gridConsumer {
  return _gridConsumer;
}

#pragma mark - parent class methods

- (void)configureToolbarsButtons {
  // NO-OP
}

- (void)addToSelectionItemID:(GridItemIdentifier*)itemID {
  [super addToSelectionItemID:itemID];
  [_tabPickerConsumer setSelectedTabsCount:self.selectedEditingItems.tabsCount];
  [self updateDoneButtonState];
}

- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID {
  [super removeFromSelectionItemID:itemID];
  [_tabPickerConsumer setSelectedTabsCount:self.selectedEditingItems.tabsCount];
  [self updateDoneButtonState];
}

- (void)updateDoneButtonState {
  BOOL selectionChanged = self.selectedEditingItems.allTabs !=
                          [_tabsAttachmentDelegate preselectedWebStateIDs];
  [_tabPickerConsumer setDoneButtonEnabled:selectionChanged];
}

- (void)userTappedOnItemID:(GridItemIdentifier*)itemID {
  CHECK_EQ(self.modeHolder.mode, TabGridMode::kSelection);
  CHECK_EQ(itemID.type, GridItemType::kTab);
  if ([self attachmentLimitReached:itemID]) {
    [self.snackbarPresenter
        showSnackbarForTabAttachmentLimit:[_tabsAttachmentDelegate
                                              maxTabAttachmentCount]];
    return;
  }

  [super userTappedOnItemID:itemID];

  web::WebState* webState = GetWebState(
      self.webStateList,
      WebStateSearchCriteria{
          .identifier = itemID.tabSwitcherItem.identifier,
          .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned});

  if (webState) {
    base::UmaHistogramBoolean(
        "IOS.Omnibox.MobileFusebox.TabPicker.PickedWebStateIsRealized",
        webState->IsRealized());
  }

  // If the tab's APC is cached avoid updating the snapshot.
  BOOL cached = webState && _validAPCwebStatesIDs.contains(base::NumberToString(
                                webState->GetUniqueIdentifier().identifier()));
  if (webState && !webState->IsRealized() && !cached) {
    // If the web state is not realized, force it to realize in order to have
    // the latest content and updated snapshot.
    __weak TabPickerMediator* weakSelf = self;
    [_webStateDeferredExecutor
                   webState:webState
        executeOnceRealized:^{
          [weakSelf
              cancelPlaceholderForRealizedWebState:webState->GetWeakPtr()];
        }];
    // Defer snapshot update and item reconfiguration until the web state is
    // fully loaded.
    [_webStateDeferredExecutor webState:webState
                      executeOnceLoaded:^(BOOL success) {
                        if (!success) {
                          [weakSelf handleFailedTabLoad:itemID];
                          return;
                        }
                        if (!CanExtractPageContextForWebState(webState)) {
                          [weakSelf handleAttemptToAttachInvalidTab:itemID];
                          return;
                        }
                        [weakSelf
                            updateSnapshotForWebState:webState->GetWeakPtr()
                                               itemID:itemID];
                      }];
    return;
  }

  if (!CanExtractPageContextForWebState(webState)) {
    [self handleAttemptToAttachInvalidTab:itemID];
  }
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

  // Since the tab picker flattens groups to display tabs individually, this
  // method is overridden to bypass standard group cell logic.
  return [GridItemIdentifier
      tabIdentifier:webStateList->GetWebStateAt(webStateIndex)];
}

#pragma mark - TabPickerMutator

- (void)attachSelectedTabs {
  BOOL selectionChanged = self.selectedEditingItems.allTabs !=
                          [_tabsAttachmentDelegate preselectedWebStateIDs];
  if (!selectionChanged) {
    return;
  }
  std::set<web::WebStateID> cachedWebStateIDs;
  for (const auto& webStateID : self.selectedEditingItems.allTabs) {
    if (_validAPCwebStatesIDs.contains(
            base::NumberToString(webStateID.identifier()))) {
      cachedWebStateIDs.insert(webStateID);
    }
  }
  // Call this even if `selectedEditingItems` is empty as you can remove tabs
  // from tab picker.
  [_tabsAttachmentDelegate attachSelectedTabs:self
                          selectedWebStateIDs:self.selectedEditingItems.allTabs
                            cachedWebStateIDs:cachedWebStateIDs];
}

#pragma mark - Private

/// Creates grid items for all non-NTP tabs in the web state list, including
/// those within groups. The completion handler is called with the created
/// items.
- (void)createGridItemsWithCompletion:
    (void (^)(NSArray<GridItemIdentifier*>*))completion {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    if (!webState) {
      continue;
    }
    const GURL& url = webState->GetVisibleURL();
    if (!url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
    [items addObject:item];
  }

  if (completion) {
    completion(items);
  }
}

/// Fetches the page contexts for all web states in the list.
- (void)fetchPageContexts {
  PersistTabContextBrowserAgent* persistTabContextBrowserAgent =
      PersistTabContextBrowserAgent::FromBrowser(self.browser);
  if (!persistTabContextBrowserAgent) {
    return;
  }
  std::vector<std::string> webStateUniqueIDs;
  for (int i = 0; i < self.webStateList->count(); ++i) {
    webStateUniqueIDs.push_back(
        base::NumberToString(self.webStateList->GetWebStateAt(i)
                                 ->GetUniqueIdentifier()
                                 .identifier()));
  }

  __weak __typeof(self) weakSelf = self;
  persistTabContextBrowserAgent->GetMultipleContextsAsync(
      webStateUniqueIDs,
      base::BindOnce(
          ^(PersistTabContextBrowserAgent::PageContextMap pageContextMap) {
            [weakSelf didFetchPageContexts:pageContextMap];
          }));
}

/// Called when the persisted tab contexts have been fetched. This method
/// filters the web states to only include those with a valid context, and
/// updates `_validAPCwebStatesIDs` with their identifiers.
- (void)didFetchPageContexts:
    (PersistTabContextBrowserAgent::PageContextMap&)pageContextMap {
  std::set<std::string> validCachedwebStatesIDs;
  for (const auto& [webStateUniqueIDString, pageContext] : pageContextMap) {
    if (pageContext.has_value()) {
      validCachedwebStatesIDs.insert(webStateUniqueIDString);
    }
  }

  _validAPCwebStatesIDs = validCachedwebStatesIDs;
}

/// Populates the grid consumer with the given `items`. Also pre-selects any
/// items that are already marked as selected by the delegate.
- (void)populateGridItems:(NSArray<GridItemIdentifier*>*)items {
  std::set<web::WebStateID> preselectedWebStatesIDs =
      [self.tabsAttachmentDelegate preselectedWebStateIDs];
  for (GridItemIdentifier* item in items) {
    if (item.type != GridItemType::kTab) {
      continue;
    }

    web::WebStateID webStateID = item.tabSwitcherItem.identifier;

    if (!webStateID.valid()) {
      continue;
    }
    if (preselectedWebStatesIDs.contains(webStateID)) {
      [self addToSelectionItemID:item];
    }
  }

  __weak __typeof(self) weakSelf = self;
  [_gridConsumer populateItems:items
        selectedItemIdentifier:[self activeIdentifier]
                    completion:^{
                      [weakSelf bringActiveItemIntoViewAfterPopulation:items];
                    }];
}

/// Brings the active item into view after the grid has been populated.
- (void)bringActiveItemIntoViewAfterPopulation:
    (NSArray<GridItemIdentifier*>*)items {
  GridItemIdentifier* activeIdentifier = [self activeIdentifier];
  if (activeIdentifier && [items containsObject:activeIdentifier]) {
    [_gridConsumer bringItemIntoView:activeIdentifier animated:NO];
  }
}

- (void)cancelPlaceholderForRealizedWebState:
    (base::WeakPtr<web::WebState>)weakWebState {
  web::WebState* webState = weakWebState.get();
  if (!webState) {
    return;
  }
  PagePlaceholderTabHelper::FromWebState(webState)
      ->CancelPlaceholderForNextNavigation();
}

/// Updates the snapshot for the given web state and reconfigures the grid item.
- (void)updateSnapshotForWebState:(base::WeakPtr<web::WebState>)weakWebState
                           itemID:(GridItemIdentifier*)itemID {
  web::WebState* webState = weakWebState.get();
  if (!webState) {
    return;
  }

  // This function is called when the web state successfully loaded, so it is
  // not a failed loaded item anymore.
  if ([_failedLoadedItemIDs containsObject:itemID]) {
    [_failedLoadedItemIDs removeObject:itemID];
  }

  __weak TabPickerMediator* weakSelf = self;
  SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(
      ^(UIImage* image) {
        [weakSelf reconfigureGridItem:itemID];
      });
}

/// Reconfigures the grid item to reflect updated state (e.g., new snapshot).
- (void)reconfigureGridItem:(GridItemIdentifier*)itemID {
  [_gridConsumer replaceItem:itemID withReplacementItem:itemID];
}

/// Checks if the attachment quota is full. Returns YES only if the limit is
/// reached and the provided `itemID` is not already selected (since selecting
/// an existing item implies removal).
- (BOOL)attachmentLimitReached:(GridItemIdentifier*)itemID {
  return ![self.selectedEditingItems containItem:itemID] &&
         (self.selectedEditingItems.tabsCount >=
          [_tabsAttachmentDelegate maxTabAttachmentCount]);
}

/// Handles the scenario where a tab fails to load.
- (void)handleFailedTabLoad:(GridItemIdentifier*)itemID {
  [self.snackbarPresenter showCannotReloadTabError];
  [_failedLoadedItemIDs addObject:itemID];
  [self removeFromSelectionItemID:itemID];
  [self reconfigureGridItem:itemID];
}

/// Handles the scenario where a user attempts to attach an invalid tab.
- (void)handleAttemptToAttachInvalidTab:(GridItemIdentifier*)itemID {
  [self.snackbarPresenter showCannotAttachTabError];
  [self removeFromSelectionItemID:itemID];
  [self reconfigureGridItem:itemID];
}

#pragma mark - WebStateDeferredExecutorDelegate

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
                willLoadWebState:(web::WebState*)webState {
  if ([self.logger respondsToSelector:@selector(logWillLoadTabWithTitle:
                                                                  tabID:)]) {
    [self.logger
        logWillLoadTabWithTitle:base::SysUTF16ToNSString(webState->GetTitle())
                          tabID:webState->GetUniqueIdentifier()];
  }
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
                 didLoadWebState:(web::WebState*)webState
                         success:(BOOL)success {
  if ([self.logger respondsToSelector:@selector
                   (logDidLoadTabWithSuccess:title:tabID:)]) {
    [self.logger
        logDidLoadTabWithSuccess:success
                           title:base::SysUTF16ToNSString(webState->GetTitle())
                           tabID:webState->GetUniqueIdentifier()];
  }
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
        willForceRealizeWebState:(web::WebState*)webState {
  if ([self.logger respondsToSelector:@selector(logWillRealizeTabWithTitle:
                                                                     tabID:)]) {
    [self.logger logWillRealizeTabWithTitle:base::SysUTF16ToNSString(
                                                webState->GetTitle())
                                      tabID:webState->GetUniqueIdentifier()];
  }
}

- (void)webStateDeferredExecutor:(WebStateDeferredExecutor*)executor
         didForceRealizeWebState:(web::WebState*)webState {
  if ([self.logger respondsToSelector:@selector(logDidRealizeTabWithTitle:
                                                                    tabID:)]) {
    [self.logger
        logDidRealizeTabWithTitle:base::SysUTF16ToNSString(webState->GetTitle())
                            tabID:webState->GetUniqueIdentifier()];
  }
}

@end

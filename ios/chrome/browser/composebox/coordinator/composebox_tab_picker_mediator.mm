// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/coordinator/composebox_tab_picker_mediator.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_constants.h"
#import "ios/chrome/browser/composebox/coordinator/web_state_deferred_executor.h"
#import "ios/chrome/browser/composebox/ui/composebox_snackbar_presenter.h"
#import "ios/chrome/browser/composebox/ui/composebox_tab_picker_consumer.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/chrome/browser/web/model/page_placeholder_tab_helper.h"
#import "ios/web/public/web_state.h"

@implementation ComposeboxTabPickerMediator {
  /// The grid consumer.
  __weak id<TabCollectionConsumer> _gridConsumer;
  /// The tab picker consumer.
  __weak id<ComposeboxTabPickerConsumer> _tabPickerConsumer;
  /// The delegate for tabs attachment.
  __weak id<ComposeboxTabsAttachmentDelegate> _tabsAttachmentDelegate;
  /// Stores the unique identifiers of web states that have valid cached APC
  /// (Annotated Page Content) data.
  std::set<std::string> _validAPCwebStatesIDs;
  /// Stores the unique identifiers of web states that have failed to load.
  NSMutableSet<GridItemIdentifier*>* _failedLoadedItemIDs;
  /// Utility to delay execution of blocks until a web state is loaded.
  WebStateDeferredExecutor* _webStateDeferredExecutor;
}

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
                   tabPickerConsumer:
                       (id<ComposeboxTabPickerConsumer>)tabPickerConsumer
              tabsAttachmentDelegate:
                  (id<ComposeboxTabsAttachmentDelegate>)tabsAttachmentDelegate {
  TabGridModeHolder* modeHolder = [[TabGridModeHolder alloc] init];
  modeHolder.mode = TabGridMode::kSelection;
  self = [super initWithModeHolder:modeHolder];

  if (self) {
    _gridConsumer = gridConsumer;
    _tabPickerConsumer = tabPickerConsumer;
    _tabsAttachmentDelegate = tabsAttachmentDelegate;

    [_tabPickerConsumer
        setSelectedTabsCount:self.selectedEditingItems.tabsCount];

    _webStateDeferredExecutor = [[WebStateDeferredExecutor alloc] init];
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
}

- (id<TabCollectionConsumer>)gridConsumer {
  return _gridConsumer;
}

#pragma mark - parent class methods

- (BOOL)shouldShowSnapshotForItem:(GridItemIdentifier*)itemID {
  CHECK(self.modeHolder.mode == TabGridMode::kSelection);
  if (itemID.type == GridItemType::kTab) {
    web::WebState* webState = GetWebState(
        self.webStateList,
        WebStateSearchCriteria{
            .identifier = itemID.tabSwitcherItem.identifier,
            .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned});

    if (!webState) {
      return NO;
    }

    if ([_failedLoadedItemIDs containsObject:itemID] || webState->IsCrashed()) {
      return NO;
    }

    BOOL cached = _validAPCwebStatesIDs.contains(
        base::NumberToString(webState->GetUniqueIdentifier().identifier()));
    return cached || (webState->IsRealized() && !webState->IsLoading());
  }
  return [super shouldShowSnapshotForItem:itemID];
}

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
    ComposeboxSnackbarPresenter* snackbar =
        [[ComposeboxSnackbarPresenter alloc] initWithBrowser:self.browser];
    [snackbar showAttachmentLimitSnackbar];
    return;
  }

    web::WebState* webState = GetWebState(
        self.webStateList,
        WebStateSearchCriteria{
            .identifier = itemID.tabSwitcherItem.identifier,
            .pinned_state = WebStateSearchCriteria::PinnedState::kNonPinned});
    // If the tab's APC is cached avoid updating the snapshot.
    BOOL cached =
        webState && _validAPCwebStatesIDs.contains(base::NumberToString(
                        webState->GetUniqueIdentifier().identifier()));
    if (webState && !webState->IsRealized() && !cached) {
      // If the web state is not realized, force it to realize in order to have
      // the latest content and updated snapshot.
      __weak ComposeboxTabPickerMediator* weakSelf = self;
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
                          [weakSelf
                              updateSnapshotForWebState:webState->GetWeakPtr()
                                                 itemID:itemID];
                        }];
    }
  [super userTappedOnItemID:itemID];
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

#pragma mark - ComposeboxTabPickerMutator

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

#pragma mark - private

/// Creates grid items. Depending on the feature flag
/// `kComposeboxTabPickerVariation` param value, this will either fetch tabs
/// that have a persisted tab context or create items for all tabs in the web
/// state list. The completion handler is called with the created items.
- (void)createGridItemsWithCompletion:
    (void (^)(NSArray<GridItemIdentifier*>*))completion {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
    [items addObject:item];
  }

  if (completion) {
    completion(items);
  }

  PersistTabContextBrowserAgent* persistTabContextBrowserAgent =
      PersistTabContextBrowserAgent::FromBrowser(self.browser);
  if (!IsComposeboxTabPickerCachedAPCEnabled() ||
      !persistTabContextBrowserAgent) {
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
      base::BindOnce(^(
          PersistTabContextBrowserAgent::PageContextMap pageContextMap) {
        [weakSelf didFetchPageContexts:pageContextMap completion:completion];
      }));
}

/// Called when the persisted tab contexts have been fetched. This method
/// filters the web states to only include those with a valid context, creates
/// grid items for them, and then calls the completion handler.
- (void)didFetchPageContexts:
            (PersistTabContextBrowserAgent::PageContextMap&)pageContextMap
                  completion:
                      (void (^)(NSArray<GridItemIdentifier*>*))completion {
  NSMutableArray<GridItemIdentifier*>* items = [[NSMutableArray alloc] init];

  std::set<std::string> validCachedwebStatesIDs;
  for (const auto& [webStateUniqueIDString, pageContext] : pageContextMap) {
    if (pageContext.has_value()) {
      validCachedwebStatesIDs.insert(webStateUniqueIDString);
    }
  }

  _validAPCwebStatesIDs = validCachedwebStatesIDs;
  for (int i = 0; i < self.webStateList->count(); ++i) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    GridItemIdentifier* item = [GridItemIdentifier tabIdentifier:webState];
    [items addObject:item];
  }

  if (completion) {
    completion(items);
  }
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

  [_gridConsumer populateItems:items selectedItemIdentifier:nil];

  // Defer scrolling until the next run loop to ensure the collection view
  // layout is finalized.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf bringActiveGridItemIntoView];
  });
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

  __weak ComposeboxTabPickerMediator* weakSelf = self;
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
         (self.selectedEditingItems.tabsCount +
              [_tabsAttachmentDelegate nonTabAttachmentCount] >=
          kAttachmentLimit);
}

/// Handles the scenario where a tab fails to load.
- (void)handleFailedTabLoad:(GridItemIdentifier*)itemID {
  ComposeboxSnackbarPresenter* snackbar =
      [[ComposeboxSnackbarPresenter alloc] initWithBrowser:self.browser];
  [snackbar showCannotReloadTabError];
  [_failedLoadedItemIDs addObject:itemID];
  [self removeFromSelectionItemID:itemID];
  [self reconfigureGridItem:itemID];
}

/// Brings the active grid item into view.
- (void)bringActiveGridItemIntoView {
  [_gridConsumer bringItemIntoView:[self activeIdentifier] animated:NO];
}

@end

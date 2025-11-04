// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_consumer.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_switcher_item.h"
#import "ios/web/public/web_state.h"

@implementation AimPrototypeTabPickerMediator {
  /// The grid consumer.
  __weak id<TabCollectionConsumer> _gridConsumer;
  /// The tab picker consumer.
  __weak id<AimPrototypeTabPickerConsumer> _tabPickerConsumer;
  /// The delegate for tabs attachment.
  __weak id<AimPrototypeTabsAttachmentDelegate> _tabsAttachmentDelegate;
}

- (instancetype)
      initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer
         tabPickerConsumer:(id<AimPrototypeTabPickerConsumer>)tabPickerConsumer
    tabsAttachmentDelegate:
        (id<AimPrototypeTabsAttachmentDelegate>)tabsAttachmentDelegate {
  TabGridModeHolder* modeHolder = [[TabGridModeHolder alloc] init];
  modeHolder.mode = TabGridMode::kSelection;
  self = [super initWithModeHolder:modeHolder];

  if (self) {
    _gridConsumer = gridConsumer;
    _tabPickerConsumer = tabPickerConsumer;
    _tabsAttachmentDelegate = tabsAttachmentDelegate;
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

- (void)configureToolbarsButtons {
  // NO-OP
}

- (void)addToSelectionItemID:(GridItemIdentifier*)itemID {
  [super addToSelectionItemID:itemID];
  [_tabPickerConsumer setSelectedTabsCount:self.selectedEditingItems.tabsCount];
}

- (void)removeFromSelectionItemID:(GridItemIdentifier*)itemID {
  [super removeFromSelectionItemID:itemID];
  [_tabPickerConsumer setSelectedTabsCount:self.selectedEditingItems.tabsCount];
}

#pragma mark - AimPrototypeTabPickerMutator

- (void)attachSelectedTabs {
  if (self.selectedEditingItems.itemsIdentifiers.count) {
    [_tabsAttachmentDelegate
         attachSelectedTabs:self
        selectedWebStateIDs:self.selectedEditingItems.allTabs];
  }
}

#pragma mark - private

/// Creates grid items. Depending on the feature flag `kAIMPrototypeTabPicker`
/// param value, this will either fetch tabs that have a persisted tab context
/// or create items for all tabs in the web state list. The completion handler
/// is called with the created items.
- (void)createGridItemsWithCompletion:
    (void (^)(NSArray<GridItemIdentifier*>*))completion {
  if (!IsAimPrototypeTabPickerCachedAPCEnabled()) {
    completion(CreateTabItems(self.webStateList,
                              TabGroupRange(0, self.webStateList->count())));
    return;
  }

  PersistTabContextBrowserAgent* persistTabContextBrowserAgent =
      PersistTabContextBrowserAgent::FromBrowser(self.browser);
  if (!persistTabContextBrowserAgent) {
    completion([[NSArray alloc] init]);
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

  for (int i = 0; i < self.webStateList->count(); ++i) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    if (validCachedwebStatesIDs.contains(base::NumberToString(
            webState->GetUniqueIdentifier().identifier()))) {
      [items addObject:[GridItemIdentifier tabIdentifier:webState]];
    }
  }

  completion(items);
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
}

@end

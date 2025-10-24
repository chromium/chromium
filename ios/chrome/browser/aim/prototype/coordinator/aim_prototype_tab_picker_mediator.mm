// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_consumer.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
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

  if (self.webStateList) {
    NSArray<GridItemIdentifier*>* items = CreateItems(self.webStateList);
    [_gridConsumer populateItems:items selectedItemIdentifier:nil];

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
  }
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

@end

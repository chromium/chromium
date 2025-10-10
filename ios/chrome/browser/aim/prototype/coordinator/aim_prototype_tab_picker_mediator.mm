// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/selected_grid_items.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"

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
    [_gridConsumer populateItems:CreateItems(self.webStateList)
          selectedItemIdentifier:nil];
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
        selectedIdentifiers:self.selectedEditingItems.itemsIdentifiers];
  }
}

@end

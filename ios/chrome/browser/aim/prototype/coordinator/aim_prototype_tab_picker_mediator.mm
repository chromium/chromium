// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_collection_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_mode_holder.h"

@implementation AimPrototypeTabPickerMediator {
  __weak id<TabCollectionConsumer> _gridConsumer;
}

- (instancetype)initWithGridConsumer:(id<TabCollectionConsumer>)gridConsumer {
  TabGridModeHolder* modeHolder = [[TabGridModeHolder alloc] init];
  modeHolder.mode = TabGridMode::kSelection;
  self = [super initWithModeHolder:modeHolder];

  if (self) {
    _gridConsumer = gridConsumer;
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

- (void)configureToolbarsButtons {
  // NO-OP
}

@end

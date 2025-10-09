// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/coordinator/aim_prototype_tab_picker_mediator.h"

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

- (id<TabCollectionConsumer>)gridConsumer {
  return _gridConsumer;
}

@end

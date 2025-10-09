// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/prototype/ui/aim_prototype_tab_picker_view_controller.h"

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/ui/base_grid_view_controller.h"

@implementation AimPrototypeTabPickerViewController

- (instancetype)init {
  self = [super init];

  if (self) {
    _gridViewController = [[BaseGridViewController alloc] init];
    _gridViewController.theme = GridThemeLight;
    [_gridViewController setTabGridMode:TabGridMode::kSelection];
  }

  return self;
}

@end

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_mediator.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

@implementation TabGridToolbarsMediator {
  TabGridToolbarsConfiguration* _configuration;
}

#pragma mark - GridToolbarsMutator

- (void)setToolbarConfiguration:(TabGridToolbarsConfiguration*)configuration {
  _configuration = configuration;
}

@end

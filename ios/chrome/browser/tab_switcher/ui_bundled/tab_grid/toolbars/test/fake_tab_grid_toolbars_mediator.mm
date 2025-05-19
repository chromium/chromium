// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"

@implementation FakeTabGridToolbarsMediator

- (void)setToolbarConfiguration:(TabGridToolbarsConfiguration*)configuration {
  self.configuration = configuration;
}

- (void)setToolbarsButtonsDelegate:(id<TabGridToolbarsGridDelegate>)delegate {
  self.delegate = delegate;
}

- (void)setButtonsEnabled:(BOOL)enabled {
  self.enabled = enabled;
}

- (void)setIncognitoToolbarsBackgroundHidden:(BOOL)hidden {
  self.incognitoHidden = hidden;
}

@end

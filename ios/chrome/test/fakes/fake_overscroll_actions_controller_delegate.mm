// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/fakes/fake_overscroll_actions_controller_delegate.h"

@implementation FakeOverscrollActionsControllerDelegate

- (instancetype)init {
  self = [super init];
  if (self) {
    _headerView = [[UIView alloc] init];
  }
  return self;
}

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  _selectedAction = OverscrollAction::NEW_TAB;
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  _selectedAction = OverscrollAction::CLOSE_TAB;
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  _selectedAction = OverscrollAction::REFRESH;
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return nil;
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return _headerView;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return 0;
}

- (CGFloat)initialContentOffsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return 0;
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return nullptr;
}

@end

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/undo_manager_wrapper.h"

#include <memory>

#include "components/undo/bookmark_undo_service.h"
#include "components/undo/undo_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/bookmarks/undo_manager_bridge_observer.h"
#include "ios/chrome/browser/undo/bookmark_undo_service_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UndoManagerWrapper ()<UndoManagerBridgeObserver> {
  std::unique_ptr<bookmarks::UndoManagerBridge> _bridge;
}
@property(nonatomic, assign) UndoManager* undoManager;
@property(nonatomic, assign) BOOL hasUndoManagerChanged;
@end

@implementation UndoManagerWrapper
@synthesize hasUndoManagerChanged = _hasUndoManagerChanged;
@synthesize undoManager = _undoManager;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _undoManager =
        ios::BookmarkUndoServiceFactory::GetForBrowserState(browserState)
            ->undo_manager();
    _bridge.reset(new bookmarks::UndoManagerBridge(self));
    _undoManager->AddObserver(_bridge.get());
  }
  return self;
}

- (void)dealloc {
  _undoManager->RemoveObserver(_bridge.get());
}

#pragma mark - Public Methods

- (void)startGroupingActions {
  self.undoManager->StartGroupingActions();
}

- (void)stopGroupingActions {
  self.undoManager->EndGroupingActions();
}

- (void)resetUndoManagerChanged {
  self.hasUndoManagerChanged = NO;
}

- (void)undo {
  self.undoManager->Undo();
}

#pragma mark - UndoManagerBridgeObserver

- (void)undoManagerChanged {
  self.hasUndoManagerChanged = YES;
}

@end

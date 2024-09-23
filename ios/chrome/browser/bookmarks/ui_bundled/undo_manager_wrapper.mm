// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/undo_manager_wrapper.h"

#import <memory>

#import "components/undo/bookmark_undo_service.h"
#import "components/undo/undo_manager.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_undo_service_factory.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/undo_manager_bridge_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface UndoManagerWrapper () <UndoManagerBridgeObserver> {
  std::unique_ptr<bookmarks::UndoManagerBridge> _bridge;
}
@property(nonatomic, assign) UndoManager* undoManager;
@property(nonatomic, assign) BOOL hasUndoManagerChanged;
@end

@implementation UndoManagerWrapper
@synthesize hasUndoManagerChanged = _hasUndoManagerChanged;
@synthesize undoManager = _undoManager;

- (instancetype)initWithBrowserState:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    _undoManager =
        ios::BookmarkUndoServiceFactory::GetForProfile(profile)->undo_manager();
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

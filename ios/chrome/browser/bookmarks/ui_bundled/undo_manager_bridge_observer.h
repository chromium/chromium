// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_BRIDGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_BRIDGE_OBSERVER_H_

#include "components/undo/undo_manager_observer.h"

// The ObjC translations of the C++ observer callbacks are defined here.
@protocol UndoManagerBridgeObserver
// Invoked when the internal state of the undo manager has changed.
- (void)undoManagerChanged;
@end

namespace bookmarks {
// A bridge that translates UndoManagerObserver C++ callbacks into ObjC
// callbacks.
class UndoManagerBridge : public UndoManagerObserver {
 public:
  explicit UndoManagerBridge(id<UndoManagerBridgeObserver> observer);
  ~UndoManagerBridge() override {}

 private:
  void OnUndoManagerStateChange() override;
  __weak id<UndoManagerBridgeObserver> observer_;
};
}  // namespace bookmarks

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_BRIDGE_OBSERVER_H_

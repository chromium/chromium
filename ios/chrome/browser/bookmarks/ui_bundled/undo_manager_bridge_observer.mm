// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/undo_manager_bridge_observer.h"

#import "base/check.h"

namespace bookmarks {
UndoManagerBridge::UndoManagerBridge(id<UndoManagerBridgeObserver> observer)
    : observer_(observer) {
  DCHECK(observer);
}

void UndoManagerBridge::OnUndoManagerStateChange() {
  [observer_ undoManagerChanged];
}
}  // namespace bookmarks

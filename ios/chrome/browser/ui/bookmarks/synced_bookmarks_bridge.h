// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_SYNCED_BOOKMARKS_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_SYNCED_BOOKMARKS_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/macros.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"

namespace signin {
class IdentityManager;
}

namespace ios {
class ChromeBrowserState;
}

namespace sync_bookmarks {

// Bridge class that will notify the panel when the remote bookmarks content
// change.
class SyncedBookmarksObserverBridge : public SyncObserverBridge {
 public:
  SyncedBookmarksObserverBridge(id<SyncObserverModelBridge> delegate,
                                ios::ChromeBrowserState* browserState);
  ~SyncedBookmarksObserverBridge() override;
  // Returns true if user is signed in.
  bool IsSignedIn();
  // Returns true if it is undergoing the first sync cycle.
  bool IsPerformingInitialSync();

 private:
  signin::IdentityManager* identity_manager_;
  ios::ChromeBrowserState* browser_state_;

  DISALLOW_COPY_AND_ASSIGN(SyncedBookmarksObserverBridge);
};

}  // namespace sync_bookmarks

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_SYNCED_BOOKMARKS_BRIDGE_H_

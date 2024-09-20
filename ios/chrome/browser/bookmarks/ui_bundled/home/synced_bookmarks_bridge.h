// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_SYNCED_BOOKMARKS_BRIDGE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_SYNCED_BOOKMARKS_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"

namespace signin {
class IdentityManager;
}

namespace sync_bookmarks {

// Bridge class that will notify the panel when the remote bookmarks content
// change.
class SyncedBookmarksObserverBridge : public SyncObserverBridge {
 public:
  SyncedBookmarksObserverBridge(id<SyncObserverModelBridge> delegate,
                                ProfileIOS* profile);

  SyncedBookmarksObserverBridge(const SyncedBookmarksObserverBridge&) = delete;
  SyncedBookmarksObserverBridge& operator=(
      const SyncedBookmarksObserverBridge&) = delete;

  ~SyncedBookmarksObserverBridge() override;
  // Returns true if it is undergoing the first sync cycle.
  bool IsPerformingInitialSync();

 private:
  raw_ptr<signin::IdentityManager> identity_manager_;
  base::WeakPtr<ProfileIOS> profile_;
};

}  // namespace sync_bookmarks

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_SYNCED_BOOKMARKS_BRIDGE_H_

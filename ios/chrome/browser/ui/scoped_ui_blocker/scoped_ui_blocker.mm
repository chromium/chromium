// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_manager.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_target.h"

ScopedUIBlocker::ScopedUIBlocker(id<UIBlockerTarget> target) : target_(target) {
  DCHECK(target_);
  id<UIBlockerManager> uiBlockerManager = target.uiBlockerManager;
  DCHECK(uiBlockerManager);
  [uiBlockerManager incrementBlockingUICounterForTarget:target_];
}

ScopedUIBlocker::~ScopedUIBlocker() {
  DCHECK(target_) << "Cannot unlock the blocking UI if scene is deallocated.";
  [target_.uiBlockerManager decrementBlockingUICounterForTarget:target_];
}

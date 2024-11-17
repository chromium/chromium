// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_manager.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/ui_blocker_target.h"

ScopedUIBlocker::ScopedUIBlocker(id<UIBlockerTarget> target,
                                 UIBlockerExtent extent)
    : target_(target) {
  DCHECK(target_);
  uiBlockerManager_ = [target uiBlockerManagerForExtent:extent];
  DCHECK(uiBlockerManager_);
  [uiBlockerManager_ incrementBlockingUICounterForTarget:target_];
}

ScopedUIBlocker::~ScopedUIBlocker() {
  DCHECK(target_) << "Cannot unlock the blocking UI if scene is deallocated.";
  [uiBlockerManager_ decrementBlockingUICounterForTarget:target_];
}

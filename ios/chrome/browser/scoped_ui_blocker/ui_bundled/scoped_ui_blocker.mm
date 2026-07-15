// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/scoped_ui_blocker.h"

#import "base/check.h"
#import "base/logging.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_manager.h"
#import "ios/chrome/browser/scoped_ui_blocker/ui_bundled/ui_blocker_target.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

ScopedUIBlocker::ScopedUIBlocker(PassKey,
                                 id<UIBlockerTarget> target,
                                 id<UIBlockerManager> manager)
    : target_(target), manager_(manager) {
  DCHECK(target_);
  DCHECK(manager_);
  [manager_ incrementBlockingUICounterForTarget:target_];
}

ScopedUIBlocker::~ScopedUIBlocker() {
  DCHECK(target_) << "Cannot unlock the blocking UI if scene is deallocated.";
  [manager_ decrementBlockingUICounterForTarget:target_];
}

// static
std::unique_ptr<ScopedUIBlocker> ScopedUIBlocker::ProfileScoped(
    SceneState* scene) {
  return std::make_unique<ScopedUIBlocker>(PassKey{}, scene,
                                           scene.profileState);
}

// static
std::unique_ptr<ScopedUIBlocker> ScopedUIBlocker::AppScoped(
    SceneState* scene,
    AppState* app_state) {
  return std::make_unique<ScopedUIBlocker>(PassKey{}, scene, app_state);
}

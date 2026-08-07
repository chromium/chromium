// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/public/scoped_force_fullscreen.h"

#import "base/check.h"

ScopedForceFullscreen::ScopedForceFullscreen(id<FullscreenCommands> handler,
                                             ForceFullscreenFeature feature)
    : handler_(handler), feature_(feature) {
  CHECK(handler_);
  [handler_ forceFullscreen:YES feature:feature_];
}

ScopedForceFullscreen::~ScopedForceFullscreen() {
  // During browser shutdown, FullscreenCoordinator may be stopped and
  // unregistered from the CommandDispatcher before this scoped object is
  // destroyed. Check if the handler still responds to the selector to avoid
  // unrecognized selector crashes during teardown.
  if ([(id)handler_ respondsToSelector:@selector(forceFullscreen:feature:)]) {
    [handler_ forceFullscreen:NO feature:feature_];
  }
}

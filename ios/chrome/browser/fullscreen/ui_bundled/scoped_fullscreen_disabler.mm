// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/scoped_fullscreen_disabler.h"

ScopedFullscreenDisabler::ScopedFullscreenDisabler(
    FullscreenController* controller)
    : scoped_observer_(this), controller_(controller) {
  DCHECK(controller_);
  scoped_observer_.Observe(controller);
  controller_->IncrementDisabledCounter();
}

ScopedFullscreenDisabler::ScopedFullscreenDisabler(
    id<FullscreenCommands> handler,
    bool animated)
    : handler_(handler) {
  DCHECK(handler_);
  [handler_ disableFullscreenAnimated:animated];
}

ScopedFullscreenDisabler::~ScopedFullscreenDisabler() {
  if (controller_) {
    controller_->DecrementDisabledCounter();
  }
  // During browser shutdown, FullscreenCoordinator may be stopped and
  // unregistered from the CommandDispatcher before this disabler is destroyed.
  // Check if the handler still responds to the selector to avoid unrecognized
  // selector crashes during teardown.
  if ([(id)handler_ respondsToSelector:@selector(reenableFullscreen)]) {
    [handler_ reenableFullscreen];
  }
}

void ScopedFullscreenDisabler::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  DCHECK(scoped_observer_.IsObservingSource(controller));
  scoped_observer_.Reset();
  controller_ = nullptr;
}

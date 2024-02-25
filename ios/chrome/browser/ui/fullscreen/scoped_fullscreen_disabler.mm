// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/scoped_fullscreen_disabler.h"

ScopedFullscreenDisabler::ScopedFullscreenDisabler(
    FullscreenController* controller)
    : scoped_observer_(this), controller_(controller) {
  DCHECK(controller_);
  scoped_observer_.Observe(controller);
  controller_->IncrementDisabledCounter();
}

ScopedFullscreenDisabler::~ScopedFullscreenDisabler() {
  if (controller_)
    controller_->DecrementDisabledCounter();
}

void ScopedFullscreenDisabler::FullscreenControllerWillShutDown(
    FullscreenController* controller) {
  DCHECK(scoped_observer_.IsObservingSource(controller));
  scoped_observer_.Reset();
  controller_ = nullptr;
}

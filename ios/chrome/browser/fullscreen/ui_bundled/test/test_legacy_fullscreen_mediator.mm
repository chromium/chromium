// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_legacy_fullscreen_mediator.h"

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"

#pragma mark - TestNoopAnimationProvider

class TestNoopAnimationProvider : public FullscreenControllerObserver {
 public:
  // FullscreenControllerObserver:
  void FullscreenWillAnimate(FullscreenController* controller,
                             FullscreenAnimator* animator) override {
    [animator addAnimations:^{
    }];
  }
};

#pragma mark - TestLegacyFullscreenMediator

TestLegacyFullscreenMediator::TestLegacyFullscreenMediator(
    FullscreenController* controller,
    FullscreenModel* model)
    : LegacyFullscreenMediator(controller, model),
      noopAnimationProvider_(std::make_unique<TestNoopAnimationProvider>()) {
  AddObserver(noopAnimationProvider_.get());
}

TestLegacyFullscreenMediator::~TestLegacyFullscreenMediator() {
  RemoveObserver(noopAnimationProvider_.get());
}

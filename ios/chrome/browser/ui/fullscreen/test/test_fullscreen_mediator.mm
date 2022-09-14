// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_mediator.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

#pragma mark - TestFullscreenMediator

TestFullscreenMediator::TestFullscreenMediator(FullscreenController* controller,
                                               FullscreenModel* model)
    : FullscreenMediator(controller, model),
      noopAnimationProvider_(std::make_unique<TestNoopAnimationProvider>()) {
  AddObserver(noopAnimationProvider_.get());
}

TestFullscreenMediator::~TestFullscreenMediator() {
  RemoveObserver(noopAnimationProvider_.get());
}

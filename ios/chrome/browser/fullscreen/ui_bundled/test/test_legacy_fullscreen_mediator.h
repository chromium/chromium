// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_LEGACY_FULLSCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_LEGACY_FULLSCREEN_MEDIATOR_H_

#import <memory>

#import "ios/chrome/browser/fullscreen/ui_bundled/legacy_fullscreen_mediator.h"

class FullscreenControllerObserver;

// Test LegacyFullscreenMediator that prevents throwing exceptions when starting
// animators with no animations.
class TestLegacyFullscreenMediator : public LegacyFullscreenMediator {
 public:
  TestLegacyFullscreenMediator(FullscreenController* controller,
                               FullscreenModel* model);
  ~TestLegacyFullscreenMediator() override;

 private:
  // The test observer that provides no-op animation blocks to animators.
  std::unique_ptr<FullscreenControllerObserver> noopAnimationProvider_;
};

#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_TEST_TEST_LEGACY_FULLSCREEN_MEDIATOR_H_

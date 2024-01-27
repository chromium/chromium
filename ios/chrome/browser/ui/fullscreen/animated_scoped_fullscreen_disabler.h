// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

class FullscreenController;

// A helper object that increments FullscrenController's disabled counter for
// its entire lifetime after calling StartAnimation().  Any UI updates resulting
// from the incremented disable counter will be animated.
class AnimatedScopedFullscreenDisabler : public FullscreenControllerObserver {
 public:
  explicit AnimatedScopedFullscreenDisabler(FullscreenController* controller);
  ~AnimatedScopedFullscreenDisabler() override;

  // Starts the disabling the FullscreenController, animating any resulting UI
  // changes.  The FullscreenController will then remain disabled until this
  // disabler is deallocated.
  void StartAnimation();

  // FullscreenControllerObserver implementation.
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;

 private:
  // Helper method used to implement the animations.
  void OnAnimationStart();

  // The FullscreenController being disabled by this object.
  raw_ptr<FullscreenController> controller_ = nullptr;

  // Whether this disabler is contributing to `controller_`'s disabled counter.
  bool disabling_ = false;
  // Used to implement animation blocks safely.
  base::WeakPtrFactory<AnimatedScopedFullscreenDisabler> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

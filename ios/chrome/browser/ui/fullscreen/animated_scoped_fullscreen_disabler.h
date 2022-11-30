// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

class FullscreenController;
class AnimatedScopedFullscreenDisablerObserver;
@class AnimatedScopedFullscreenDisablerObserverListContainer;

// A helper object that increments FullscrenController's disabled counter for
// its entire lifetime after calling StartAnimation().  Any UI updates resulting
// from the incremented disable counter will be animated.
class AnimatedScopedFullscreenDisabler : public FullscreenControllerObserver {
 public:
  explicit AnimatedScopedFullscreenDisabler(FullscreenController* controller);
  ~AnimatedScopedFullscreenDisabler() override;

  // Adds and removes AnimatedScopedFullscreenDisablerObservers.
  void AddObserver(AnimatedScopedFullscreenDisablerObserver* observer);
  void RemoveObserver(AnimatedScopedFullscreenDisablerObserver* observer);

  // Starts the disabling the FullscreenController, animating any resulting UI
  // changes.  The FullscreenController will then remain disabled until this
  // disabler is deallocated.
  void StartAnimation();

  // FullscreenControllerObserver implementation.
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;

 private:
  // Helper methods used to implement the animations.
  void OnAnimationStart();
  void OnAnimationCompletion();

  // The FullscreenController being disabled by this object.
  FullscreenController* controller_ = nullptr;
  // A container object for the list of observers.
  __strong AnimatedScopedFullscreenDisablerObserverListContainer*
      observer_list_container_ = nil;
  // Whether this disabler is contributing to `controller_`'s disabled counter.
  bool disabling_ = false;
  // Used to implement animation blocks safely.
  base::WeakPtrFactory<AnimatedScopedFullscreenDisabler> weak_factory_{this};
};

// Obsever class for listening to animated fullscreen disabling events.
class AnimatedScopedFullscreenDisablerObserver {
 public:
  explicit AnimatedScopedFullscreenDisablerObserver() = default;
  virtual ~AnimatedScopedFullscreenDisablerObserver() = default;

  // Called when the fullscreen disabling animation begins and ends.  If
  // AnimatedScopedFullscreenDisabler::StartAnimation() is called and for a
  // FullscreenController that is already disabled, these callbacks will not be
  // sent.  If the disabler is destroyed before the animation can finish,
  // FullscreenDisablingAnimationDidFinish() will not be received.
  virtual void FullscreenDisablingAnimationDidStart(
      AnimatedScopedFullscreenDisabler* disabler) {}
  virtual void FullscreenDisablingAnimationDidFinish(
      AnimatedScopedFullscreenDisabler* disabler) {}

  // Called before the disabler is destructed.
  virtual void AnimatedFullscreenDisablerDestroyed(
      AnimatedScopedFullscreenDisabler* disabler) {}
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

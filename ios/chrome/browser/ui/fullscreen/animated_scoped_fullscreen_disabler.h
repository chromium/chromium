// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_ANIMATED_SCOPED_FULLSCREEN_DISABLER_H_

#include "base/observer_list.h"

class FullscreenController;
class AnimatedScopedFullscreenDisablerObserver;
@class AnimatedScopedFullscreenDisablerObserverListContainer;

// A helper object that increments FullscrenController's disabled counter for
// its entire lifetime after calling StartAnimation().  Any UI updates resulting
// from the incremented disable counter will be animated.
class AnimatedScopedFullscreenDisabler {
 public:
  explicit AnimatedScopedFullscreenDisabler(FullscreenController* controller);

  AnimatedScopedFullscreenDisabler(const AnimatedScopedFullscreenDisabler&) =
      delete;
  AnimatedScopedFullscreenDisabler& operator=(
      const AnimatedScopedFullscreenDisabler&) = delete;

  ~AnimatedScopedFullscreenDisabler();

  // Adds and removes AnimatedScopedFullscreenDisablerObservers.
  void AddObserver(AnimatedScopedFullscreenDisablerObserver* observer);
  void RemoveObserver(AnimatedScopedFullscreenDisablerObserver* observer);

  // Starts the disabling the FullscreenController, animating any resulting UI
  // changes.  The FullscreenController will then remain disabled until this
  // disabler is deallocated.
  void StartAnimation();

 private:
  // The FullscreenController being disabled by this object.
  FullscreenController* controller_;
  // A container object for the list of observers.
  __strong AnimatedScopedFullscreenDisablerObserverListContainer*
      observer_list_container_;
  // Whether this disabler is contributing to |controller_|'s disabled counter.
  bool disabling_ = false;
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

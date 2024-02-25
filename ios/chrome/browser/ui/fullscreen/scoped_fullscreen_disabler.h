// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_SCOPED_FULLSCREEN_DISABLER_H_

#include "base/check.h"
#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

// A helper object that increments FullscrenController's disabled counter for
// its entire lifetime.
class ScopedFullscreenDisabler : public FullscreenControllerObserver {
 public:
  explicit ScopedFullscreenDisabler(FullscreenController* controller);

  ScopedFullscreenDisabler(const ScopedFullscreenDisabler&) = delete;
  ScopedFullscreenDisabler& operator=(const ScopedFullscreenDisabler&) = delete;

  ~ScopedFullscreenDisabler() override;

 private:
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;

  // Scoped observer that facilitates observing an FullscreenController.
  base::ScopedObservation<FullscreenController, FullscreenControllerObserver>
      scoped_observer_{this};
  // The FullscreenController being disabled by this object.
  raw_ptr<FullscreenController> controller_;
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_SCOPED_FULLSCREEN_DISABLER_H_

// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_SCOPED_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_SCOPED_FULLSCREEN_DISABLER_H_

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller_observer.h"
#import "ios/chrome/browser/shared/public/commands/fullscreen_commands.h"

// A helper object that increments the fullscreen's disabled counter for
// its entire lifetime. It can disable either a FullscreenController (legacy)
// or use FullscreenCommands.
class ScopedFullscreenDisabler : public FullscreenControllerObserver {
 public:
  // TODO(crbug.com/490126971): Clean up legacy FullscreenController constructor
  // once refactor is complete.
  explicit ScopedFullscreenDisabler(FullscreenController* controller);
  explicit ScopedFullscreenDisabler(id<FullscreenCommands> handler,
                                    bool animated = true);

  ScopedFullscreenDisabler(const ScopedFullscreenDisabler&) = delete;
  ScopedFullscreenDisabler& operator=(const ScopedFullscreenDisabler&) = delete;

  ~ScopedFullscreenDisabler() override;

 private:
  // FullscreenControllerObserver:
  void FullscreenControllerWillShutDown(
      FullscreenController* controller) override;

  // Scoped observer that facilitates observing a FullscreenController.
  base::ScopedObservation<FullscreenController, FullscreenControllerObserver>
      scoped_observer_{this};
  // The FullscreenController being disabled by this object (legacy).
  raw_ptr<FullscreenController> controller_ = nullptr;
  // The FullscreenCommands being disabled by this object.
  __weak id<FullscreenCommands> handler_ = nil;
};
#endif  // IOS_CHROME_BROWSER_FULLSCREEN_UI_BUNDLED_SCOPED_FULLSCREEN_DISABLER_H_

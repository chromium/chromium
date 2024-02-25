// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_UPDATER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_UPDATER_H_

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_observer.h"

@protocol FullscreenUIElement;

// Forwards signals received via FullscreenControllerObserver callbacks to
// FullscreenUIElements.
class FullscreenUIUpdater {
 public:
  // Constructor for an updater that updates `ui_element` for observed events
  // from `controller`.  Both arguments must be non-null.  `ui_element` is not
  // retained.  The updater will observe `controller` until the controller is
  // shut down or the updater is destroyed.
  FullscreenUIUpdater(FullscreenController* controller,
                      id<FullscreenUIElement> ui_element);
  ~FullscreenUIUpdater();

 private:
  // Stops observing `controller_`.
  void Disconnect();

  // Helper object that forwards FullscreenControllerObserver callbacks to their
  // FullscreenUIElement counterparts.
  class FullscreenControllerObserverForwarder
      : public FullscreenControllerObserver {
   public:
    // Constructor for a forwarder that updates `ui_element` for `updater`.
    FullscreenControllerObserverForwarder(FullscreenUIUpdater* updater,
                                          id<FullscreenUIElement> ui_element);

    // FullscreenControllerObserver:
    void FullscreenViewportInsetRangeChanged(
        FullscreenController* controller,
        UIEdgeInsets min_viewport_insets,
        UIEdgeInsets max_viewport_insets) override;
    void FullscreenProgressUpdated(FullscreenController* controller,
                                   CGFloat progress) override;
    void FullscreenEnabledStateChanged(FullscreenController* controller,
                                       bool enabled) override;
    void FullscreenWillAnimate(FullscreenController* controller,
                               FullscreenAnimator* animator) override;
    void FullscreenControllerWillShutDown(
        FullscreenController* controller) override;

   private:
    raw_ptr<FullscreenUIUpdater> updater_ = nullptr;
    __weak id<FullscreenUIElement> ui_element_ = nil;
  };

  // The FullscreenController being observed.
  raw_ptr<FullscreenController> controller_ = nullptr;
  // The observer forwarder.
  FullscreenControllerObserverForwarder forwarder_;
  // Scoped observer for `forwarder_`.
  base::ScopedObservation<FullscreenController, FullscreenControllerObserver>
      observation_{&forwarder_};
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_UI_UPDATER_H_

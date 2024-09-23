// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_FULLSCREEN_DISABLER_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_FULLSCREEN_DISABLER_H_

#include <memory>

#import "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer.h"

class Browser;
class AnimatedScopedFullscreenDisabler;
class FullscreenController;

// A helper object that disables fullscreen while overlays are displayed.
class OverlayContainerFullscreenDisabler {
 public:
  // Constructs a OverlayContainerFullscreenDisabler that disables fullscreen
  // for `browser` when overlays are displayed at `modality`.
  OverlayContainerFullscreenDisabler(Browser* browser,
                                     OverlayModality modality);
  ~OverlayContainerFullscreenDisabler();
  OverlayContainerFullscreenDisabler(
      OverlayContainerFullscreenDisabler& disabler) = delete;

 private:
  // Helper object that disables fullscreen when overlays are presented.
  class FullscreenDisabler : public OverlayPresenterObserver {
   public:
    FullscreenDisabler(FullscreenController* fullscreen_controller,
                       OverlayPresenter* overlay_presenter);
    ~FullscreenDisabler() override;

   private:
    // OverlayPresenterObserver:
    void WillShowOverlay(OverlayPresenter* presenter,
                         OverlayRequest* request,
                         bool initial_presentation) override;
    void DidHideOverlay(OverlayPresenter* presenter,
                        OverlayRequest* request) override;
    void OverlayPresenterDestroyed(OverlayPresenter* presenter) override;

    // The FullscreenController being disabled.
    raw_ptr<FullscreenController> fullscreen_controller_ = nullptr;
    // The animated disabler.
    std::unique_ptr<AnimatedScopedFullscreenDisabler> disabler_;
    base::ScopedObservation<OverlayPresenter, OverlayPresenterObserver>
        scoped_observation_{this};
  };

  FullscreenDisabler fullscreen_disabler_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_OVERLAY_PRESENTATION_CONTEXT_FULLSCREEN_DISABLER_H_

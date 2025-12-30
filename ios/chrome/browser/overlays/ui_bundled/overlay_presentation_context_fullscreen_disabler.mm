// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_context_fullscreen_disabler.h"

#import "base/check.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/animated_scoped_fullscreen_disabler.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

#pragma mark - OverlayContainerFullscreenDisabler

OverlayContainerFullscreenDisabler::OverlayContainerFullscreenDisabler(
    Browser* browser,
    OverlayModality modality)
    : fullscreen_disabler_(browser, modality) {}

OverlayContainerFullscreenDisabler::~OverlayContainerFullscreenDisabler() =
    default;

#pragma mark - OverlayContainerFullscreenDisabler::FullscreenDisabler

OverlayContainerFullscreenDisabler::FullscreenDisabler::FullscreenDisabler(
    Browser* browser,
    OverlayModality modality)
    : browser_(browser) {
  OverlayPresenter* overlay_presenter =
      OverlayPresenter::FromBrowser(browser, modality);
  DCHECK(overlay_presenter);
  scoped_observation_.Observe(overlay_presenter);
}

OverlayContainerFullscreenDisabler::FullscreenDisabler::~FullscreenDisabler() =
    default;

void OverlayContainerFullscreenDisabler::FullscreenDisabler::WillShowOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request,
    bool initial_presentation) {
  disabler_ = std::make_unique<AnimatedScopedFullscreenDisabler>(
      FullscreenController::FromBrowser(browser_));
  disabler_->StartAnimation();
}

void OverlayContainerFullscreenDisabler::FullscreenDisabler::DidHideOverlay(
    OverlayPresenter* presenter,
    OverlayRequest* request) {
  disabler_ = nullptr;
}

void OverlayContainerFullscreenDisabler::FullscreenDisabler::
    OverlayPresenterDestroyed(OverlayPresenter* presenter) {
  DCHECK(scoped_observation_.IsObservingSource(presenter));
  scoped_observation_.Reset();
  disabler_ = nullptr;
}

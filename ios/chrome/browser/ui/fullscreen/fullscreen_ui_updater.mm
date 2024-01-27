// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"

#import "ios/chrome/browser/ui/fullscreen/fullscreen_animator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

FullscreenUIUpdater::FullscreenUIUpdater(FullscreenController* controller,
                                         id<FullscreenUIElement> ui_element)
    : controller_(controller),
      forwarder_(this, ui_element),
      observation_(&forwarder_) {
  DCHECK(controller_);
  observation_.Observe(controller_.get());
}

FullscreenUIUpdater::~FullscreenUIUpdater() = default;

void FullscreenUIUpdater::Disconnect() {
  if (!controller_)
    return;
  DCHECK(observation_.IsObservingSource(controller_.get()));
  observation_.Reset();
  controller_ = nullptr;
}

FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenControllerObserverForwarder(FullscreenUIUpdater* updater,
                                          id<FullscreenUIElement> ui_element)
    : updater_(updater), ui_element_(ui_element) {
  DCHECK(updater_);
  DCHECK(ui_element_);
}

void FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenProgressUpdated(FullscreenController* controller,
                              CGFloat progress) {
  [ui_element_ updateForFullscreenProgress:progress];
}

void FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenViewportInsetRangeChanged(FullscreenController* controller,
                                        UIEdgeInsets min_viewport_insets,
                                        UIEdgeInsets max_viewport_insets) {
  SEL inset_range_selector = @selector(updateForFullscreenMinViewportInsets:
                                                          maxViewportInsets:);
  if ([ui_element_ respondsToSelector:inset_range_selector]) {
    [ui_element_ updateForFullscreenMinViewportInsets:min_viewport_insets
                                    maxViewportInsets:max_viewport_insets];
  }
}

void FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenEnabledStateChanged(FullscreenController* controller,
                                  bool enabled) {
  if ([ui_element_ respondsToSelector:@selector(updateForFullscreenEnabled:)]) {
    [ui_element_ updateForFullscreenEnabled:enabled];
  } else if (!enabled) {
    [ui_element_ updateForFullscreenProgress:1.0];
  }
}

void FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenWillAnimate(FullscreenController* controller,
                          FullscreenAnimator* animator) {
  SEL animator_selector = @selector(animateFullscreenWithAnimator:);
  if ([ui_element_ respondsToSelector:animator_selector]) {
    [ui_element_ animateFullscreenWithAnimator:animator];
  } else {
    CGFloat progress = animator.finalProgress;
    [animator addAnimations:^{
      [ui_element_ updateForFullscreenProgress:progress];
    }];
  }
}

void FullscreenUIUpdater::FullscreenControllerObserverForwarder::
    FullscreenControllerWillShutDown(FullscreenController* controller) {
  updater_->Disconnect();
}

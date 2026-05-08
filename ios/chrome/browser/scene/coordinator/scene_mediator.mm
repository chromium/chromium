// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/scene/coordinator/scene_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service.h"
#import "ios/chrome/browser/scene/ui/scene_consumer.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"

@implementation SceneMediator {
  raw_ptr<FullscreenController> _regularFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _regularFullscreenUIUpdater;
  raw_ptr<FullscreenController> _incognitoFullscreenController;
  std::unique_ptr<FullscreenUIUpdater> _incognitoFullscreenUIUpdater;
}

- (instancetype)initWithRegularFullscreenController:
                    (FullscreenController*)regularFullscreenController
                      incognitoFullscreenController:
                          (FullscreenController*)incognitoFullscreenController {
  self = [super init];
  if (self) {
    _regularFullscreenController = regularFullscreenController;
    _incognitoFullscreenController = incognitoFullscreenController;
    _appBarPositionAtLaunch = AppBarPosition::kNone;
  }
  return self;
}

- (void)setConsumer:(id<FullscreenUIElement, SceneConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  _regularFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
      _regularFullscreenController, _consumer);
  if (_incognitoFullscreenController) {
    _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _incognitoFullscreenController, _consumer);
  }
  [self triggerNewIAPromo];
}

- (void)disconnect {
  self.consumer = nil;
  _regularFullscreenUIUpdater.reset();
  _incognitoFullscreenUIUpdater.reset();
  _regularFullscreenController = nullptr;
  _incognitoFullscreenController = nullptr;
  self.tracker = nullptr;
}

- (void)setIncognitoFullscreenController:
    (FullscreenController*)incognitoFullscreenController {
  _incognitoFullscreenUIUpdater.reset();
  _incognitoFullscreenController = incognitoFullscreenController;
  if (_incognitoFullscreenController && _consumer) {
    _incognitoFullscreenUIUpdater = std::make_unique<FullscreenUIUpdater>(
        _incognitoFullscreenController, _consumer);
  }
}

#pragma mark - SceneMutator

- (void)newIAPromoIPHDismissed {
  if (self.tracker) {
    self.tracker->Dismissed(feature_engagement::kIPHiOSNewIAPromoFeature);
  }
}

- (void)setappBarPositionAtLaunch:(AppBarPosition)appBarPositionAtLaunch {
  if (_appBarPositionAtLaunch == appBarPositionAtLaunch) {
    return;
  }
  _appBarPositionAtLaunch = appBarPositionAtLaunch;
  [self triggerNewIAPromo];
}

#pragma mark - Private

// Check if the new IA IPH promo can be shown.
- (void)triggerNewIAPromo {
  if (!IsChromeNextIaEnabled()) {
    return;
  }
  if (!self.tracker) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  _tracker->AddOnInitializedCallback(base::BindOnce(^(bool success) {
    if (!success) {
      return;
    }
    [weakSelf showNewIAPromo];
  }));
}

// Triggers the new IA IPH promo if the conditions are met.
- (void)showNewIAPromo {
  if (!self.consumer) {
    return;
  }
  if (self.appBarPositionAtLaunch == AppBarPosition::kNone) {
    return;
  }
  if (_tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHiOSNewIAPromoFeature)) {
    BOOL eligible =
        _geminiService && _geminiService->IsProfileEligibleForGemini();
    [self.consumer showNewIAPromoWithGeminiEligibility:eligible];
  }
}

@end

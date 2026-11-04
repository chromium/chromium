// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_coordinator.h"

#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_mediator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/coordinator/ttc_voice_plate_mediator.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_keyed_service_factory.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_view_controller.h"
#import "ios/chrome/browser/ai_prototyping/ttc/ui/ttc_voice_plate_view.h"
#import "ios/chrome/browser/ai_prototyping/utils/ai_prototyping_constants.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation TTCCoordinator {
  // View controller for TTC.
  TTCViewController* _viewController;

  // Mediator for TTC journal.
  TTCMediator* _mediator;

  // Mediator for TTC voice plate.
  TTCVoicePlateMediator* _voicePlateMediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController =
      [[TTCViewController alloc] initForFeature:AIPrototypingFeature::kTTC];

  TTCKeyedService* ttcService =
      self.profile ? TTCKeyedServiceFactory::GetForProfile(self.profile)
                   : nullptr;
  _mediator = [[TTCMediator alloc] initWithTTCService:ttcService];
  _voicePlateMediator =
      [[TTCVoicePlateMediator alloc] initWithTTCService:ttcService];

  _mediator.consumer = _viewController;
  _viewController.ttcMutator = _mediator;

  TTCVoicePlateView* voicePlateView = _viewController.voicePlateView;
  _voicePlateMediator.consumer = voicePlateView;
  voicePlateView.mutator = _voicePlateMediator;
}

- (void)stop {
  [_voicePlateMediator disconnect];
  _voicePlateMediator = nil;
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - Public

- (UIViewController<AIPrototypingViewControllerProtocol>*)viewController {
  return _viewController;
}

@end

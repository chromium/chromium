// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_view_controller.h"

@implementation BrandingCoordinator {
  // Mediator that handles branding visibility and animation.
  BrandingMediator* _mediator;
}

- (void)start {
  _viewController = [[BrandingViewController alloc] init];
  _mediator = [[BrandingMediator alloc]
      initWithLocalState:GetApplicationContext()->GetLocalState()];

  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

@end

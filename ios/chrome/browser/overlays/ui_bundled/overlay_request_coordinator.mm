// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"

@interface OverlayRequestCoordinator () <OverlayRequestMediatorDelegate> {
  // Subclassing properties.
  BOOL _started;
  OverlayRequestMediator* _mediator;
}
@end

@implementation OverlayRequestCoordinator

- (void)dealloc {
  // ChromeCoordinator's `-dealloc` calls `-stop`, which defaults to an animated
  // dismissal.  OverlayRequestCoordinators should instead stop without
  // animation so that the OverlayRequestCoordinatorDelegate can be notified of
  // the dismissal immediately.
  [self stopAnimated:NO];
}

+ (const OverlayRequestSupport*)requestSupport {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
  return OverlayRequestSupport::None();
}

+ (BOOL)showsOverlayUsingChildViewController {
  return NO;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                   request:(OverlayRequest*)request
                                  delegate:(OverlayRequestCoordinatorDelegate*)
                                               delegate {
  DCHECK([self class].requestSupport->IsRequestSupported(request));
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _request = request;
    DCHECK(_request);
    _delegate = delegate;
    DCHECK(_delegate);
  }
  return self;
}

#pragma mark - Public

- (void)startAnimated:(BOOL)animated {
  NOTREACHED_IN_MIGRATION() << "Subclasses must implement.";
}

- (void)stopAnimated:(BOOL)animated {
  NOTREACHED_IN_MIGRATION() << "Subclasses must implement.";
}

#pragma mark - ChromeCoordinator

- (void)start {
  [self startAnimated:YES];
}

- (void)stop {
  [self stopAnimated:YES];
}

#pragma mark - OverlayRequestMediatorDelegate

- (void)stopOverlayForMediator:(OverlayRequestMediator*)mediator {
  [self stopAnimated:YES];
}

@end

@implementation OverlayRequestCoordinator (Subclassing)

- (void)setStarted:(BOOL)started {
  _started = started;
}

- (BOOL)isStarted {
  return _started;
}

- (void)setMediator:(OverlayRequestMediator*)mediator {
  _mediator.delegate = nil;
  _mediator = mediator;
  _mediator.delegate = self;
}

- (OverlayRequestMediator*)mediator {
  return _mediator;
}

@end

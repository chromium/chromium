// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator+subclassing.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/overlays/public/overlay_request_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  NOTREACHED() << "Subclasses implement.";
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
  NOTREACHED() << "Subclasses must implement.";
}

- (void)stopAnimated:(BOOL)animated {
  NOTREACHED() << "Subclasses must implement.";
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

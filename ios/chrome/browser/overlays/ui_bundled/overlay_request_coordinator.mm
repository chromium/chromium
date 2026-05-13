// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator.h"

#import <ostream>

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator+subclassing.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_coordinator_delegate.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"

@interface OverlayRequestCoordinator () <OverlayRequestMediatorDelegate>
@end

@implementation OverlayRequestCoordinator {
  // Subclassing properties.
  BOOL _started;
  OverlayRequestMediator* _mediator;
  raw_ptr<OverlayRequest> _request;
  OverlayRequestId _requestId;
}

- (void)dealloc {
  // ChromeCoordinator's `-dealloc` calls `-stop`, which defaults to an animated
  // dismissal.  OverlayRequestCoordinators should instead stop without
  // animation so that the OverlayRequestCoordinatorDelegate can be notified of
  // the dismissal immediately.
  [self stopAnimated:NO];
}

+ (const OverlayRequestSupport*)requestSupport {
  NOTREACHED() << "Subclasses implement.";
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
    _requestId = request->GetRequestId();
    _delegate = delegate;
    DCHECK(_delegate);

    // Register a completion callback to synchronously null-out `_request` as
    // soon as the request is completed or destroyed. This ensures that
    // `_request` never becomes a dangling raw_ptr.
    __weak __typeof(self) weakSelf = self;
    _request->GetCallbackManager()->AddCompletionCallback(
        base::BindOnce(^(OverlayResponse*) {
          [weakSelf requestWasCompleted];
        }));
  }
  return self;
}

- (OverlayRequest*)request {
  return _request.get();
}

- (OverlayRequestId)requestId {
  return _requestId;
}

- (void)requestWasCompleted {
  _request = nullptr;
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

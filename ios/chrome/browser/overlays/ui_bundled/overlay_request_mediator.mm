// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

#import <ostream>

#import "base/functional/bind.h"
#import "base/notreached.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"

@interface OverlayRequestMediator ()
// Redefine property as readwrite.
@property(nonatomic, readwrite) OverlayRequest* request;
@end

@implementation OverlayRequestMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if ((self = [super init])) {
    _request = request;
    _request->GetCallbackManager()->AddCompletionCallback(
        [self requestCompletionCallback]);
  }
  return self;
}

#pragma mark - Public

+ (const OverlayRequestSupport*)requestSupport {
  NOTREACHED_IN_MIGRATION() << "Subclasses implement.";
  return OverlayRequestSupport::None();
}

#pragma mark - Private

// Returns an OverlayCompletionCallback to reset the request pointer upon
// destruction to prevent it from being used in the event of overlay UI user
// interaction events that occur during dismissal after a request has been
// cancelled.
- (OverlayCompletionCallback)requestCompletionCallback {
  __weak __typeof(self) weakSelf = self;
  return base::BindOnce(^(OverlayResponse*) {
    weakSelf.request = nullptr;
  });
}

@end

@implementation OverlayRequestMediator (Subclassing)

- (void)dispatchResponse:(std::unique_ptr<OverlayResponse>)response {
  if (self.request)
    self.request->GetCallbackManager()->DispatchResponse(std::move(response));
}

- (void)dismissOverlay {
  [self.delegate stopOverlayForMediator:self];
}

@end

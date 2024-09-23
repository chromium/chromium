// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"

#import <UIKit/UIKit.h>

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_support.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

@implementation InfobarBannerOverlayMediator

- (instancetype)initWithRequest:(OverlayRequest*)request {
  if ((self = [super initWithRequest:request])) {
    DCHECK([self class].requestSupport->IsRequestSupported(request));
  }
  return self;
}

- (void)finishDismissal {
  // No-op as default.
}

#pragma mark - Accessors

- (void)setConsumer:(id<InfobarBannerConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  if (_consumer)
    [self configureConsumer];
}

#pragma mark - InfobarBannerDelegate

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Notify the model layer to perform the infobar's main action before
  // dismissing the banner.
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarBannerMainActionResponse>()];
  [self dismissOverlay];
}

- (void)dismissInfobarBannerForUserInteraction:(BOOL)userInitiated {
  if (userInitiated) {
    // Notify the model layer of user-initiated banner dismissal before
    // dismissing the banner.
    [self dispatchResponse:OverlayResponse::CreateWithInfo<
                               InfobarBannerUserInitiatedDismissalResponse>()];
  }
  [self dismissOverlay];
}

- (void)presentInfobarModalFromBanner {
  // Notify the model layer to show the infobar modal.  The banner is not
  // dismissed immediately, but will be cancelled upon the completion of the
  // modal UI.
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarBannerShowModalResponse>()];
}

- (void)infobarBannerWasDismissed {
  // Only needed in legacy implementation.  Dismissal completion cleanup occurs
  // in InfobarBannerOverlayCoordinator.
  // TODO(crbug.com/40668195): Remove once non-overlay implementation is
  // deleted.
}

@end

@implementation InfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  NOTREACHED_IN_MIGRATION() << "Subclasses must implement.";
}

@end

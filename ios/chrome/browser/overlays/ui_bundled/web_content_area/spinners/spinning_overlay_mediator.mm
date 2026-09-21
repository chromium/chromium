// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/web_content_area/spinners/spinning_overlay_mediator.h"

#import "ios/chrome/browser/overlays/model/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/overlays/model/public/web_content_area/spinning_overlay_request_config.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

@interface SpinningOverlayMediator ()
// The request config for the overlay.
@property(nonatomic, readonly) SpinningOverlayRequestConfig* config;
@end

@implementation SpinningOverlayMediator

#pragma mark - Accessors

- (SpinningOverlayRequestConfig*)config {
  return self.request ? self.request->GetConfig<SpinningOverlayRequestConfig>()
                      : nullptr;
}

#pragma mark - SpinningOverlayViewDelegate

- (void)spinningOverlayViewDidTap:(SpinningOverlayView*)view {
  SpinningOverlayRequestConfig* config = self.config;
  if (!config || !config->is_cancellable() || !self.request) {
    return;
  }
  self.request->GetCallbackManager()->SetCompletionResponse(
      OverlayResponse::CreateWithInfo<SpinningOverlayResponse>(
          /*canceled=*/true));
  [self dismissOverlay];
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SpinningOverlayRequestConfig::RequestSupport();
}

@end

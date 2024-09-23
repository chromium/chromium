// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/infobar_modal_overlay_mediator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/overlays/model/public/infobar_modal/infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"

@implementation InfobarModalOverlayMediator

#pragma mark - InfobarModalDelegate

- (void)dismissInfobarModal:(id)infobarModal {
  base::RecordAction(base::UserMetricsAction(kInfobarModalCancelButtonTapped));
  [self.delegate stopOverlayForMediator:self];
}

- (void)modalInfobarButtonWasAccepted:(id)infobarModal {
  [self dispatchResponse:OverlayResponse::CreateWithInfo<
                             InfobarModalMainActionResponse>()];
  [self dismissOverlay];
}

- (void)modalInfobarWasDismissed:(id)infobarModal {
  // Only needed in legacy implementation.  Dismissal completion cleanup occurs
  // in InfobarModalOverlayCoordinator.
  // TODO(crbug.com/40668195): Remove once non-overlay implementation is
  // deleted.
}

@end

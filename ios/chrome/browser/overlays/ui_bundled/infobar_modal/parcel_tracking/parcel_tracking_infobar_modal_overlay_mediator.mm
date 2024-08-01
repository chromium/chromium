// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_modal/parcel_tracking/parcel_tracking_infobar_modal_overlay_mediator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_step.h"
#import "ios/chrome/browser/ui/infobars/modals/parcel_tracking/infobar_parcel_tracking_modal_consumer.h"

@implementation ParcelTrackingInfobarModalOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

// Returns the delegate attached to the config.
- (ParcelTrackingInfobarDelegate*)parcelTrackingInfobarDelegate {
  return static_cast<ParcelTrackingInfobarDelegate*>(self.config->delegate());
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarParcelTrackingModalDelegate

- (void)parcelTrackingTableViewControllerDidTapTrackAllButton {
  self.parcelTrackingInfobarDelegate->TrackPackages(
      /*display_infobar=*/false);
  self.parcelTrackingInfobarDelegate->SetStep(
      ParcelTrackingStep::kNewPackageTracked);
}

- (void)parcelTrackingTableViewControllerDidTapUntrackAllButton {
  self.parcelTrackingInfobarDelegate->UntrackPackages(
      /*display_infobar=*/false);
  self.parcelTrackingInfobarDelegate->SetStep(
      ParcelTrackingStep::kPackageUntracked);
}

#pragma mark - Public

- (void)setConsumer:(id<InfobarParcelTrackingModalConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  ParcelTrackingInfobarDelegate* delegate =
      [self parcelTrackingInfobarDelegate];
  ParcelTrackingStep step = delegate->GetStep();
  switch (step) {
    case ParcelTrackingStep::kPackageUntracked:
      [_consumer setParcelList:delegate->GetParcelList() withTrackingStatus:NO];
      break;
    case ParcelTrackingStep::kNewPackageTracked:
      [_consumer setParcelList:delegate->GetParcelList()
            withTrackingStatus:YES];
      break;
    case ParcelTrackingStep::kAskedToTrackPackage:
      [_consumer setParcelList:delegate->GetParcelList() withTrackingStatus:NO];
      break;
  }
}

@end

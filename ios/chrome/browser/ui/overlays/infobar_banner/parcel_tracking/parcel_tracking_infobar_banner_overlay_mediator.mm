// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/parcel_tracking/parcel_tracking_infobar_banner_overlay_mediator.h"

#import "ios/chrome/browser/overlays/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/ui/overlays/overlay_request_mediator+subclassing.h"

@interface ParcelTrackingBannerOverlayMediator ()

@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;

@end

@implementation ParcelTrackingBannerOverlayMediator

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

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  [self dismissOverlay];
  ParcelTrackingInfobarDelegate* delegate = self.parcelTrackingInfobarDelegate;
  ParcelTrackingStep step = delegate->GetStep();
  switch (step) {
    case ParcelTrackingStep::kAskedToTrackPackage:
      delegate->TrackPackages();
      break;
    case ParcelTrackingStep::kPackageUntracked:
    case ParcelTrackingStep::kNewPackageTracked:
      delegate->OpenNTP();
      break;
  }
}

@end

@implementation ParcelTrackingBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  // TODO(crbug.com/1473449): implement.
}

@end

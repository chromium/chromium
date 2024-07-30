// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/parcel_tracking/parcel_tracking_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_infobar_delegate.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  ParcelTrackingInfobarDelegate* delegate = self.parcelTrackingInfobarDelegate;
  if (!delegate) {
    return;
  }

  [self dismissOverlay];

  ParcelTrackingStep step = delegate->GetStep();
  switch (step) {
    case ParcelTrackingStep::kAskedToTrackPackage:
      delegate->TrackPackages(/*display_infobar=*/true);
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
  ParcelTrackingInfobarDelegate* delegate = self.parcelTrackingInfobarDelegate;

  ParcelTrackingStep step = delegate->GetStep();
  int numberOfParcels = delegate->GetParcelList().count;

  NSString* title;
  NSString* subtitle;
  NSString* buttonText;
  bool presentsModal;

  switch (step) {
    case ParcelTrackingStep::kNewPackageTracked:
      title = base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_NEW_PACKAGE_TRACKED_TITLE,
          numberOfParcels));
      subtitle = base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_NEW_PACKAGE_TRACKED_SUBTITLE,
          numberOfParcels));
      buttonText =
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_INFOBAR_VIEW_BUTTON);
      presentsModal = YES;
      break;
    case ParcelTrackingStep::kPackageUntracked:
      title = l10n_util::GetNSString(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_PACKAGE_UNTRACKED_TITLE);
      subtitle = l10n_util::GetNSString(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_PACKAGE_UNTRACKED_SUBTITLE);
      buttonText =
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_INFOBAR_VIEW_BUTTON);
      presentsModal = YES;
      break;
    case ParcelTrackingStep::kAskedToTrackPackage:
      title = base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_ASK_TO_TRACK_TITLE, numberOfParcels));
      subtitle = base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_PARCEL_TRACKING_INFOBAR_ASK_TO_TRACK_SUBTITLE,
          numberOfParcels));
      buttonText =
          l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_INFOBAR_TRACK_BUTTON);
      presentsModal = NO;
      break;
  }

  [self.consumer setTitleText:title];
  [self.consumer setSubtitleText:subtitle];
  [self.consumer setButtonText:buttonText];
  [self.consumer setIconImage:DefaultSymbolWithPointSize(
                                  kShippingBoxSymbol, kInfobarSymbolPointSize)];
  [self.consumer setPresentsModal:presentsModal];
}

@end

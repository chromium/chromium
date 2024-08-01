// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/autofill_address_profile/save_address_profile_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/infobar_banner_overlay_responses.h"
#import "ios/chrome/browser/overlays/model/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"
#import "ios/chrome/browser/overlays/model/public/overlay_response.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ui/base/l10n/l10n_util.h"

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

@interface SaveAddressProfileInfobarBannerOverlayMediator ()
// The save address profile banner config from the request.
@property(nonatomic, assign, readonly)
    SaveAddressProfileBannerRequestConfig* config;
@end

@implementation SaveAddressProfileInfobarBannerOverlayMediator

#pragma mark - Accessors

- (SaveAddressProfileBannerRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<SaveAddressProfileBannerRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return SaveAddressProfileBannerRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Present the modal if the save/update button is pressed.
  [self presentInfobarModalFromBanner];
}

@end

@implementation SaveAddressProfileInfobarBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  if (!self.consumer || !self.config)
    return;

  [self.consumer
      setButtonText:base::SysUTF16ToNSString(self.config->button_label_text())];
  [self.consumer
      setTitleText:base::SysUTF16ToNSString(self.config->message_text())];
  [self.consumer
      setSubtitleText:base::SysUTF16ToNSString(self.config->description())];

  if (!self.config->is_migration_to_account() &&
      (!self.config->is_profile_an_account_profile() ||
       self.config->is_update_banner())) {
    [self.consumer setSubtitleNumberOfLines:1];
  }

  [self.consumer setIconImage:CustomSymbolWithPointSize(
                                  self.config->is_migration_to_account()
                                      ? kCloudAndArrowUpSymbol
                                      : kLocationSymbol,
                                  kInfobarSymbolPointSize)];
  // This is done to hide the settings image from the banner view. The modal
  // would still be presented when the user chooses to pick the Save/Update
  // action.
  [self.consumer setPresentsModal:NO];
}

@end

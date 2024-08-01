// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/permissions/permissions_infobar_banner_overlay_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/permissions/model/permissions_infobar_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_consumer.h"
#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_mediator+consumer_support.h"
#import "ios/chrome/browser/overlays/ui_bundled/overlay_request_mediator+subclassing.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface PermissionsBannerOverlayMediator ()
// The permissions banner config from the request.
@property(nonatomic, readonly) DefaultInfobarOverlayRequestConfig* config;
@end

@implementation PermissionsBannerOverlayMediator

#pragma mark - Accessors

- (DefaultInfobarOverlayRequestConfig*)config {
  return self.request
             ? self.request->GetConfig<DefaultInfobarOverlayRequestConfig>()
             : nullptr;
}

#pragma mark - OverlayRequestMediator

+ (const OverlayRequestSupport*)requestSupport {
  return DefaultInfobarOverlayRequestConfig::RequestSupport();
}

#pragma mark - InfobarOverlayRequestMediator

- (void)bannerInfobarButtonWasPressed:(UIButton*)sender {
  // Present the modal if the 'Edit' button is pressed.
  [self presentInfobarModalFromBanner];
}

@end

@implementation PermissionsBannerOverlayMediator (ConsumerSupport)

- (void)configureConsumer {
  DefaultInfobarOverlayRequestConfig* config = self.config;
  if (!self.consumer || !config) {
    return;
  }

  BOOL isCameraAccessible = NO;
  NSString* titleText;

  PermissionsInfobarDelegate* delegate =
      static_cast<PermissionsInfobarDelegate*>(config->delegate());
  NSArray<NSNumber*>* accessiblePermissions =
      delegate->GetMostRecentlyAccessiblePermissions();

  if ([accessiblePermissions containsObject:@(web::PermissionCamera)]) {
    // Camera access is enabled.
    isCameraAccessible = true;
    titleText =
        [accessiblePermissions containsObject:@(web::PermissionMicrophone)]
            ? l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_AND_MICROPHONE_ACCESSIBLE)
            : l10n_util::GetNSString(
                  IDS_IOS_PERMISSIONS_INFOBAR_BANNER_CAMERA_ACCESSIBLE);
  } else {
    // Only microphone access is enabled.
    titleText = l10n_util::GetNSString(
        IDS_IOS_PERMISSIONS_INFOBAR_BANNER_MICROPHONE_ACCESSIBLE);
  }
  NSString* buttonText = l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE);

  UIImage* iconImage =
      isCameraAccessible ? CustomSymbolWithPointSize(kCameraFillSymbol,
                                                     kInfobarSymbolPointSize)
                         : DefaultSymbolWithPointSize(kMicrophoneFillSymbol,
                                                      kInfobarSymbolPointSize);

  [self.consumer setTitleText:titleText];
  [self.consumer setButtonText:buttonText];

  [self.consumer setIconImage:iconImage];
  [self.consumer setPresentsModal:NO];
}

@end

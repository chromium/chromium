// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SYNC_ERROR_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SYNC_ERROR_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#import <UIKit/UIKit.h>
#import <string>

#import "ios/chrome/browser/overlays/public/infobar_banner/confirm_infobar_banner_overlay_request_config_storage.h"
#import "ios/chrome/browser/overlays/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace sync_error_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a SyncErrorInfoBarDelegate.
class SyncErrorBannerRequestConfig
    : public OverlayRequestConfig<SyncErrorBannerRequestConfig>,
      public confirm_infobar_overlays::ConfirmBannerRequestConfigStorage {
 public:
  ~SyncErrorBannerRequestConfig() override;

  // The icon image's tint color.
  UIColor* icon_image_tint_color() const { return icon_image_tint_color_; }

  // The icon's background tint color.
  UIColor* background_tint_color() const { return icon_background_color_; }

 private:
  OVERLAY_USER_DATA_SETUP(SyncErrorBannerRequestConfig);
  explicit SyncErrorBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The icon's tint color.
  UIColor* icon_image_tint_color_;
  // The icon's background tint color.
  UIColor* icon_background_color_;
};

}  // namespace sync_error_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SYNC_ERROR_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#import "ios/chrome/browser/overlays/model/public/infobar_banner/confirm_infobar_banner_overlay_request_config_storage.h"

#import "components/infobars/core/confirm_infobar_delegate.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/model/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace confirm_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a ConfirmInfoBarDelegate.
class ConfirmBannerRequestConfig
    : public OverlayRequestConfig<ConfirmBannerRequestConfig>,
      public ConfirmBannerRequestConfigStorage {
 public:
  ~ConfirmBannerRequestConfig() override;

 private:
  OVERLAY_USER_DATA_SETUP(ConfirmBannerRequestConfig);
  explicit ConfirmBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;
};

}  // namespace confirm_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

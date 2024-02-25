// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_PLACEHOLDER_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_PLACEHOLDER_REQUEST_CONFIG_H_

#import "ios/chrome/browser/overlays/model/public/common/placeholder_request_config.h"

#import "base/memory/raw_ptr.h"

namespace infobars {
class InfoBar;
}

// Configuration object for OverlayRequests to hold the place in the banner
// queue for an event to finish.
class InfobarBannerPlaceholderRequestConfig
    : public OverlayRequestConfig<InfobarBannerPlaceholderRequestConfig> {
 public:
  ~InfobarBannerPlaceholderRequestConfig() override;

 private:
  OVERLAY_USER_DATA_SETUP(InfobarBannerPlaceholderRequestConfig);
  explicit InfobarBannerPlaceholderRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  raw_ptr<infobars::InfoBar> infobar_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_INFOBAR_BANNER_PLACEHOLDER_REQUEST_CONFIG_H_

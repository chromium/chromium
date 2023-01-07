// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PERMISSIONS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PERMISSIONS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <UIKit/UIKit.h>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

// Configuration object for OverlayRequests for the banner UI for an Infobar.
class PermissionsBannerRequestConfig
    : public OverlayRequestConfig<PermissionsBannerRequestConfig> {
 public:
  ~PermissionsBannerRequestConfig() override;

  // The title text.
  NSString* title_text() const { return title_text_; }

  // The button text.
  NSString* button_text() const { return button_text_; }

  // The state of the camera permission.
  bool is_camera_accessible() const { return is_camera_accessible_; }

 private:
  OVERLAY_USER_DATA_SETUP(PermissionsBannerRequestConfig);
  explicit PermissionsBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  NSString* title_text_;
  NSString* button_text_;
  bool is_camera_accessible_ = false;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PERMISSIONS_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

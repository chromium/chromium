// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PASSWORD_INFOBAR_BANNER_OVERLAY_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PASSWORD_INFOBAR_BANNER_OVERLAY_H_

#include <CoreFoundation/CoreFoundation.h>

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}
class IOSChromeSavePasswordInfoBarDelegate;

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a IOSChromeSavePasswordInfoBarDelegate.
class PasswordInfobarBannerOverlayRequestConfig
    : public OverlayRequestConfig<PasswordInfobarBannerOverlayRequestConfig> {
 public:
  ~PasswordInfobarBannerOverlayRequestConfig() override;

  // The infobar delegate's title text.
  NSString* title() const { return title_; }
  // The infobar delegate's subtitle text.
  NSString* subtitle() const { return subtitle_; }
  // The infobar delegate's accessibility text. This is used when the subtitle
  // includes a hidden password text. Other than this, it will be null and the
  // default value of accessibility label will be used.
  NSString* customAccessibilityLabel() const {
    return custom_accessibility_label_;
  }
  // The text to show on the banner's confirm button.
  NSString* button_text() const { return button_text_; }
  // The action to take with the password for the requested banner view.
  password_modal::PasswordAction action() const { return action_; }

 private:
  OVERLAY_USER_DATA_SETUP(PasswordInfobarBannerOverlayRequestConfig);
  explicit PasswordInfobarBannerOverlayRequestConfig(
      infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s save passwords delegate.
  NSString* title_ = nil;
  NSString* subtitle_ = nil;
  NSString* custom_accessibility_label_ = nil;
  NSString* button_text_ = nil;
  password_modal::PasswordAction action_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_PASSWORD_INFOBAR_BANNER_OVERLAY_H_

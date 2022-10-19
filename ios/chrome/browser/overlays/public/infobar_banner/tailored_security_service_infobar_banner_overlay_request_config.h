// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TAILORED_SECURITY_SERVICE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TAILORED_SECURITY_SERVICE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#import <CoreFoundation/CoreFoundation.h>

#import <string>

#import "ios/chrome/browser/overlays/public/overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/overlay_user_data.h"
#import "ios/chrome/browser/safe_browsing/tailored_security/tailored_security_service_infobar_delegate.h"

namespace infobars {
class InfoBar;
}

namespace tailored_security_service_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a TailoredSecurityServiceInfobarDelegate.
class TailoredSecurityServiceBannerRequestConfig
    : public OverlayRequestConfig<TailoredSecurityServiceBannerRequestConfig> {
 public:
  ~TailoredSecurityServiceBannerRequestConfig() override;

  // The message text.
  std::u16string message_text() const { return message_text_; }

  // The button label text.
  std::u16string button_label_text() const { return button_label_text_; }

  // The description.
  std::u16string description() const { return description_; }

  // The badge of the infobar.
  bool has_badge() const { return has_badge_; }

  // The message state.
  safe_browsing::TailoredSecurityServiceMessageState message_state() const {
    return message_state_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(TailoredSecurityServiceBannerRequestConfig);
  explicit TailoredSecurityServiceBannerRequestConfig(
      infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s tailored security delegate.
  std::u16string message_text_;
  std::u16string description_;
  std::u16string button_label_text_;
  safe_browsing::TailoredSecurityServiceMessageState message_state_;
  // Determines if the banner should show the gear icon.
  bool has_badge_ = false;
};

}  // namespace tailored_security_service_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_TAILORED_SECURITY_SERVICE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

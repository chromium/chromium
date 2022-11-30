// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_ADDRESS_PROFILE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_ADDRESS_PROFILE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace autofill_address_profile_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a AutofillSaveUpdateAddressProfileDelegateIOS.
class SaveAddressProfileBannerRequestConfig
    : public OverlayRequestConfig<SaveAddressProfileBannerRequestConfig> {
 public:
  ~SaveAddressProfileBannerRequestConfig() override;

  // The message text.
  std::u16string message_text() const { return message_text_; }

  // The button label text.
  std::u16string button_label_text() const { return button_label_text_; }

  // The description.
  std::u16string description() const { return description_; }

  // The name of the icon image.
  NSString* icon_image_name() const { return icon_image_name_; }

  // The banner type.
  BOOL is_update_banner() const { return is_update_banner_; }

 private:
  OVERLAY_USER_DATA_SETUP(SaveAddressProfileBannerRequestConfig);
  explicit SaveAddressProfileBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s save address profile
  // delegate.
  std::u16string message_text_;
  std::u16string description_;
  std::u16string button_label_text_;
  NSString* icon_image_name_ = nil;

  // Determines the type of the banner, true for save and false for the update.
  bool is_update_banner_ = false;
};

}  // namespace autofill_address_profile_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_ADDRESS_PROFILE_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

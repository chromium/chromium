// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include <string>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace save_card_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a AutofillSaveCardInfoBarDelegateMobile.
class SaveCardBannerRequestConfig
    : public OverlayRequestConfig<SaveCardBannerRequestConfig> {
 public:
  ~SaveCardBannerRequestConfig() override;

  // The message text.
  std::u16string message_text() const { return message_text_; }

  // The label for the card.
  std::u16string card_label() const { return card_label_; }

  // The card holder name of the card.
  std::u16string cardholder_name() const { return cardholder_name_; }

  // The expiration month of the card.
  std::u16string expiration_date_month() const {
    return expiration_date_month_;
  }

  // The expiration year of the card.
  std::u16string expiration_date_year() const { return expiration_date_year_; }

  // The button label text.
  std::u16string button_label_text() const { return button_label_text_; }

  // Whether the action is an upload or a local save.
  bool should_upload_credentials() const { return should_upload_credentials_; }

 private:
  OVERLAY_USER_DATA_SETUP(SaveCardBannerRequestConfig);
  explicit SaveCardBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s save card delegate.
  std::u16string message_text_;
  std::u16string card_label_;
  std::u16string cardholder_name_;
  std::u16string expiration_date_month_;
  std::u16string expiration_date_year_;
  std::u16string button_label_text_;
  bool should_upload_credentials_ = false;
};

}  // namespace save_card_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

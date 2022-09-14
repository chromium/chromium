// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <string>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"
#include "ui/gfx/image/image.h"

namespace infobars {
class InfoBar;
}

namespace confirm_infobar_overlays {

// Configuration object for OverlayRequests for the banner UI for an InfoBar
// with a ConfirmInfoBarDelegate.
class ConfirmBannerRequestConfig
    : public OverlayRequestConfig<ConfirmBannerRequestConfig> {
 public:
  ~ConfirmBannerRequestConfig() override;

  // The title text.
  std::u16string title_text() const { return title_text_; }

  // The message text.
  std::u16string message_text() const { return message_text_; }

  // The button label text.
  std::u16string button_label_text() const { return button_label_text_; }

  // The infobar's icon image.
  gfx::Image icon_image() const { return icon_image_; }

  // Whether to present the Infobar's banner for a longer amount of time.
  bool is_high_priority() const { return is_high_priority_; }

  // Whether to use a background tint for the icon image.
  bool use_icon_background_tint() const { return use_icon_background_tint_; }

 private:
  OVERLAY_USER_DATA_SETUP(ConfirmBannerRequestConfig);
  explicit ConfirmBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s confirm delegate.
  std::u16string title_text_;
  std::u16string message_text_;
  std::u16string button_label_text_;
  gfx::Image icon_image_;
  // True if the icon image should apply a background tint.
  bool use_icon_background_tint_ = true;
  // True if the infobar's banner should be presented for a longer time.
  bool is_high_priority_ = false;
};

}  // namespace confirm_infobar_overlays

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

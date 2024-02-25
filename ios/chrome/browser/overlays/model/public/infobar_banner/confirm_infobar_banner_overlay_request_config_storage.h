// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_STORAGE_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_STORAGE_H_

#import <string>

#import "base/memory/raw_ptr.h"
#import "ui/gfx/image/image.h"

namespace infobars {
class InfoBar;
}

namespace confirm_infobar_overlays {

// Storage for the configuration data that can be set by a
// ConfirmInfoBarDelegate or its derived delegate types. Used as the base class
// of OverlayRequests Config types for the banner UI for an InfoBar whose
// delegate is ConfirmInfoBarDelegate or its derived types.
class ConfirmBannerRequestConfigStorage {
 public:
  ~ConfirmBannerRequestConfigStorage();

  // The title text.
  const std::u16string title_text() const { return title_text_; }

  // The message text.
  const std::u16string message_text() const { return message_text_; }

  // The button label text.
  const std::u16string button_label_text() const { return button_label_text_; }

  // The infobar's icon image.
  const gfx::Image icon_image() const { return icon_image_; }

  // Whether to present the Infobar's banner for a longer amount of time.
  bool is_high_priority() const { return is_high_priority_; }

  // Whether to use a background tint for the icon image.
  bool use_icon_background_tint() const { return use_icon_background_tint_; }

  infobars::InfoBar* infobar() const { return infobar_; }

 protected:
  explicit ConfirmBannerRequestConfigStorage(infobars::InfoBar* infobar);

 private:
  // The InfoBar causing this banner.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;

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
#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_INFOBAR_BANNER_CONFIRM_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_STORAGE_H_

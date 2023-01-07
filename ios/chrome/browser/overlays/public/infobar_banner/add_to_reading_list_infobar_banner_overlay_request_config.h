// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_ADD_TO_READING_LIST_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_ADD_TO_READING_LIST_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"
#include "ios/chrome/browser/overlays/public/overlay_user_data.h"

namespace infobars {
class InfoBar;
}

namespace reading_list_infobar_overlay {

// Configuration object for OverlayRequests for the banner UI for an Infobar
// with a IOSAddToReadingListInfobarDelegate.
class ReadingListBannerRequestConfig
    : public OverlayRequestConfig<ReadingListBannerRequestConfig> {
 public:
  ~ReadingListBannerRequestConfig() override;

  // The title text.
  NSString* title_text() const { return title_text_; }

  // The message text.
  NSString* message_text() const { return message_text_; }

  // The button text.
  NSString* button_text() const { return button_text_; }

 private:
  OVERLAY_USER_DATA_SETUP(ReadingListBannerRequestConfig);
  explicit ReadingListBannerRequestConfig(infobars::InfoBar* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  NSString* title_text_;
  NSString* message_text_;
  NSString* button_text_;

  // The InfoBar causing this banner.
  infobars::InfoBar* infobar_ = nullptr;
};

}  // namespace reading_list_infobar_overlay

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_BANNER_ADD_TO_READING_LIST_INFOBAR_BANNER_OVERLAY_REQUEST_CONFIG_H_

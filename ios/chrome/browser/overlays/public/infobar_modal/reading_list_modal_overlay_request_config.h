// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

class InfoBarIOS;

// Configuration object for OverlayRequests for the modal UI for an infobar
// with a IOSAddToReadingListInfobarDelegate.
class ReadingListInfobarModalOverlayRequestConfig
    : public OverlayRequestConfig<ReadingListInfobarModalOverlayRequestConfig> {
 public:
  ~ReadingListInfobarModalOverlayRequestConfig() override;

  bool current_page_added() const { return current_page_added_; }

 private:
  OVERLAY_USER_DATA_SETUP(ReadingListInfobarModalOverlayRequestConfig);
  explicit ReadingListInfobarModalOverlayRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;

  // Whether the current page has been added to Reading List.
  bool current_page_added_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_READING_LIST_MODAL_OVERLAY_REQUEST_CONFIG_H_

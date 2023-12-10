// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_DEFAULT_DEFAULT_INFOBAR_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_DEFAULT_DEFAULT_INFOBAR_OVERLAY_REQUEST_CONFIG_H_

#import "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_type.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_config.h"

// Default configuration object for OverlayRequests for an InfoBar.
class DefaultInfobarOverlayRequestConfig
    : public OverlayRequestConfig<DefaultInfobarOverlayRequestConfig> {
 public:
  ~DefaultInfobarOverlayRequestConfig() override;

  // Returns the delegate attached to the InfoBar.
  infobars::InfoBarDelegate* delegate() const;

  // Returns the InfobarType of the InfoBar.
  InfobarType infobar_type() const {
    return weak_infobar_.get()->infobar_type();
  }

 private:
  OVERLAY_USER_DATA_SETUP(DefaultInfobarOverlayRequestConfig);
  explicit DefaultInfobarOverlayRequestConfig(InfoBarIOS* infobar,
                                              InfobarOverlayType overlay_type);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this overlay.
  base::WeakPtr<InfoBarIOS> weak_infobar_ = nullptr;
  // Type of Overlay.
  const InfobarOverlayType overlay_type_;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_MODEL_PUBLIC_DEFAULT_DEFAULT_INFOBAR_OVERLAY_REQUEST_CONFIG_H_

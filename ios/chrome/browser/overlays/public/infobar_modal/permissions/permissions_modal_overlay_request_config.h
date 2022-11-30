// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

class InfoBarIOS;

// Configuration object for OverlayRequests for the modal UI for an infobar.
class PermissionsInfobarModalOverlayRequestConfig
    : public OverlayRequestConfig<PermissionsInfobarModalOverlayRequestConfig> {
 public:
  ~PermissionsInfobarModalOverlayRequestConfig() override;

  // The associated web state.
  web::WebState* GetWebState() const { return web_state_; }

  // The permissions description being displayed in the InfobarModal.
  NSString* GetPermissionsDescription() const {
    return permissions_description_;
  }

 private:
  OVERLAY_USER_DATA_SETUP(PermissionsInfobarModalOverlayRequestConfig);
  explicit PermissionsInfobarModalOverlayRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;

  web::WebState* web_state_ = nullptr;

  NSString* permissions_description_ = nil;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PERMISSIONS_PERMISSIONS_MODAL_OVERLAY_REQUEST_CONFIG_H_

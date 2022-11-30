// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_PERMISSIONS_OVERLAY_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_PERMISSIONS_OVERLAY_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace web {
class WebState;
}  // namespace web

// An interface derived from ConfirmInfoBarDelegate implemented by objects
// wishing to a PermissionsInfoBar.
class PermissionsOverlayInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  PermissionsOverlayInfobarDelegate(
      NSArray<NSNumber*>* recently_accessible_permissions,
      web::WebState* web_state);

  ~PermissionsOverlayInfobarDelegate() override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  InfoBarIdentifier GetIdentifier() const override;

  // Returns an array containing most recently accessible permissions to be
  // displayed in an infobar banner.
  NSArray<NSNumber*>* GetMostRecentlyAccessiblePermissions();

  // Returns the web state associated with the infobar.
  web::WebState* GetWebState() const;

 private:
  NSArray<NSNumber*>* recently_accessible_permissions_;

  web::WebState* web_state_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_PERMISSIONS_OVERLAY_INFOBAR_DELEGATE_H_

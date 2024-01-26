// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_INFOBAR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace web {
class WebState;
}  // namespace web

// An interface derived from ConfirmInfoBarDelegate implemented by objects
// wishing to a PermissionsInfoBar.
class PermissionsInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  PermissionsInfobarDelegate(
      NSArray<NSNumber*>* recently_accessible_permissions,
      web::WebState* web_state);

  ~PermissionsInfobarDelegate() override;

  // ConfirmInfoBarDelegate implementation.
  std::u16string GetMessageText() const override;
  InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(infobars::InfoBarDelegate* delegate) const override;

  // Returns an array containing most recently accessible permissions to be
  // displayed in an infobar banner.
  NSArray<NSNumber*>* GetMostRecentlyAccessiblePermissions();

  // Returns the web state associated with the infobar.
  web::WebState* GetWebState() const;

 private:
  NSArray<NSNumber*>* recently_accessible_permissions_;

  raw_ptr<web::WebState> web_state_;
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_PERMISSIONS_INFOBAR_DELEGATE_H_

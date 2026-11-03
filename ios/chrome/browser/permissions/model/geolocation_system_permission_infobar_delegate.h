// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_DELEGATE_H_

#import <string>

#import "base/memory/weak_ptr.h"
#import "base/types/pass_key.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/models/image_model.h"
#import "ui/gfx/image/image.h"

namespace web {
class WebState;
}  // namespace web

// Infobar delegate that displays a banner when a site requests geolocation
// but Chrome has location permissions denied. Tapping "Continue" triggers the
// alert dialog directing the user to iOS Settings.
class GeolocationSystemPermissionInfoBarDelegate
    : public ConfirmInfoBarDelegate {
 public:
  // Creates a GeolocationSystemPermissionInfoBar and adds it to `web_state`'s
  // InfoBarManager. Returns whether the infobar was successfully created and
  // added.
  static bool Create(web::WebState* web_state,
                     web::WebStatePermissionDecisionHandler handler);

  GeolocationSystemPermissionInfoBarDelegate(
      base::PassKey<GeolocationSystemPermissionInfoBarDelegate>,
      web::WebState* web_state,
      web::WebStatePermissionDecisionHandler handler);

  GeolocationSystemPermissionInfoBarDelegate(
      const GeolocationSystemPermissionInfoBarDelegate&) = delete;
  GeolocationSystemPermissionInfoBarDelegate& operator=(
      const GeolocationSystemPermissionInfoBarDelegate&) = delete;

  ~GeolocationSystemPermissionInfoBarDelegate() override;

  // ConfirmInfoBarDelegate implementation.
  InfoBarIdentifier GetIdentifier() const override;
  ui::ImageModel GetIcon() const override;
  std::u16string GetTitleText() const override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  void InfoBarDismissed() override;

 private:
  void DispatchDecisionIfNecessary(web::PermissionDecision decision);

  base::WeakPtr<web::WebState> web_state_;
  web::WebStatePermissionDecisionHandler handler_;
  mutable gfx::Image icon_;
};

#endif  // IOS_CHROME_BROWSER_PERMISSIONS_MODEL_GEOLOCATION_SYSTEM_PERMISSION_INFOBAR_DELEGATE_H_

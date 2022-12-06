// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_
#define IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

#include <CoreFoundation/CoreFoundation.h>

#include "ios/chrome/browser/overlays/public/overlay_request_config.h"

class InfoBarIOS;
class IOSChromeSavePasswordInfoBarDelegate;

namespace password_modal {
// The action to take for a password modal request.
enum class PasswordAction : short { kSave, kUpdate };
}  // password_modal

// Configuration object for OverlayRequests for the modal UI for an infobar
// with a IOSChromeSavePasswordInfoBarDelegate.
class PasswordInfobarModalOverlayRequestConfig
    : public OverlayRequestConfig<PasswordInfobarModalOverlayRequestConfig> {
 public:
  ~PasswordInfobarModalOverlayRequestConfig() override;

  // The action to take with the password for the requested modal view.
  password_modal::PasswordAction action() const { return action_; }
  // The username for which passwords are being saved.
  NSString* username() const { return username_; }
  // The password being saved.
  NSString* password() const { return password_; }
  // The details text.
  NSString* details_text() const { return details_text_; }
  // The text to show on the save button.
  NSString* save_button_text() const { return save_button_text_; }
  // The text to show on the cancel button.
  NSString* cancel_button_text() const { return cancel_button_text_; }
  // The URL string.
  NSString* url() const { return url_; }
  // Whether the current password has been saved.
  bool is_current_password_saved() const { return is_current_password_saved_; }

 private:
  OVERLAY_USER_DATA_SETUP(PasswordInfobarModalOverlayRequestConfig);
  explicit PasswordInfobarModalOverlayRequestConfig(InfoBarIOS* infobar);

  // OverlayUserData:
  void CreateAuxiliaryData(base::SupportsUserData* user_data) override;

  // The InfoBar causing this modal.
  InfoBarIOS* infobar_ = nullptr;
  // Configuration data extracted from `infobar_`'s save passwords delegate.
  password_modal::PasswordAction action_;
  NSString* username_ = nil;
  NSString* password_ = nil;
  NSString* details_text_ = nil;
  NSString* save_button_text_ = nil;
  NSString* cancel_button_text_ = nil;
  NSString* url_ = nil;
  bool is_current_password_saved_ = false;
};

#endif  // IOS_CHROME_BROWSER_OVERLAYS_PUBLIC_INFOBAR_MODAL_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CONFIG_H_

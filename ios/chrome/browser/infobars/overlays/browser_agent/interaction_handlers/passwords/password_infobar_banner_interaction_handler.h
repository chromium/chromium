// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import <Foundation/Foundation.h>
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

class IOSChromeSavePasswordInfoBarDelegate;

// Helper object that updates the model layer for interaction events with the
// passwords infobar banner UI.
class PasswordInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  PasswordInfobarBannerInteractionHandler(
      Browser* browser,
      password_modal::PasswordAction action_type,
      const OverlayRequestSupport* request_support);
  ~PasswordInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override;
  void MainButtonTapped(InfoBarIOS* infobar) override;

 private:
  // InfobarModalInteractionHandler:
  std::unique_ptr<InfobarBannerOverlayRequestCallbackInstaller>
  CreateBannerInstaller() override;

  // Returns the password delegate from `infobar`.
  IOSChromeSavePasswordInfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);

  // The Browser passed on initialization.
  Browser* browser_;

  // The type of Password Infobar Overlay this handler is managing.
  password_modal::PasswordAction action_type_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

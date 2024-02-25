// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_CONFIRM_CONFIRM_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_CONFIRM_CONFIRM_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

class ConfirmInfoBarDelegate;

// Helper object that updates the model layer for interaction events with the
// confirm infobar banner UI.
class ConfirmInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  ConfirmInfobarBannerInteractionHandler();
  ~ConfirmInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

 private:
  // Returns the password delegate from `infobar`.
  ConfirmInfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_CONFIRM_CONFIRM_INFOBAR_BANNER_INTERACTION_HANDLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#include <string.h>

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

class InfobarBannerOverlayRequestCallbackInstaller;

namespace autofill {
class AutofillSaveCardInfoBarDelegateMobile;
}

// Helper object that updates the model layer for interaction events with the
// SaveCard infobar banner UI.
class SaveCardInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  SaveCardInfobarBannerInteractionHandler();
  ~SaveCardInfobarBannerInteractionHandler() override;

  // Instructs the handler to update the credentials with `cardholder_name`,
  // `expiration_date_month`, and `expiration_date_year`. This replaces
  // MainButtonTapped.
  virtual void SaveCredentials(InfoBarIOS* infobar,
                               std::u16string cardholder_name,
                               std::u16string expiration_date_month,
                               std::u16string expiration_date_year);

  // Overrides InfobarBannerInteractionHandler implementation because a banner
  // dismissal should not call InfoBarDismissed();
  void BannerDismissedByUser(InfoBarIOS* infobar) override {}

 private:
  // InfobarBannerInteractionHandler:
  std::unique_ptr<InfobarBannerOverlayRequestCallbackInstaller>
  CreateBannerInstaller() override;

  // Returns the SaveCard delegate from `infobar`.
  autofill::AutofillSaveCardInfoBarDelegateMobile* GetInfobarDelegate(
      InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

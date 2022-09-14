// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

class InfoBarIOS;
class GURL;

namespace autofill {
class AutofillSaveCardInfoBarDelegateMobile;
}

// Helper object that updates the model layer for interaction events with the
// SaveCard infobar modal UI.
class SaveCardInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  SaveCardInfobarModalInteractionHandler();
  ~SaveCardInfobarModalInteractionHandler() override;

  // Instructs the handler to update the credentials with `cardholder_name`,
  // `expiration_date_month`, and `expiration_date_year`. Replaces
  // MainButtonTapped.
  virtual void UpdateCredentials(InfoBarIOS* infobar,
                                 std::u16string cardholder_name,
                                 std::u16string expiration_date_month,
                                 std::u16string expiration_date_year);

  // Instructs the handler to load `url` through the delegate.
  virtual void LoadURL(InfoBarIOS* infobar, GURL url);

  // InfobarModalInteractionHandler:
  void PerformMainAction(InfoBarIOS* infobar) override;
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override {}

 private:
  // InfobarModalInteractionHandler:
  std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
  CreateModalInstaller() override;

  // Returns the SaveCard delegate from `infobar`.
  autofill::AutofillSaveCardInfoBarDelegateMobile* GetInfoBarDelegate(
      InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_INTERACTION_HANDLER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#include <CoreFoundation/CoreFoundation.h>

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

class InfoBarIOS;

namespace autofill {
class AutofillProfile;
class AutofillSaveUpdateAddressProfileDelegateIOS;
}

// Helper object that updates the model layer for interaction events with the
// SaveAddressProfile infobar modal UI.
class SaveAddressProfileInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  SaveAddressProfileInfobarModalInteractionHandler();
  ~SaveAddressProfileInfobarModalInteractionHandler() override;

  // InfobarModalInteractionHandler:
  void PerformMainAction(InfoBarIOS* infobar) override;
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

  // Instructs the handler that the user has edited and then saved the profile.
  virtual void SaveEditedProfile(InfoBarIOS* infobar,
                                 autofill::AutofillProfile* profile);

  // Instructs the handler that the user chose not to migrate the profile.
  virtual void NoThanksWasPressed(InfoBarIOS* infobar);

  // Instructs the handler to inform the delegate that the view has been
  // cancelled.
  virtual void CancelModal(InfoBarIOS* infobar, BOOL fromEditModal);

 private:
  // InfobarModalInteractionHandler:
  std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
  CreateModalInstaller() override;

  // Returns the SaveAddressProfile delegate from `infobar`.
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* GetInfoBarDelegate(
      InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_INTERACTION_HANDLER_H_

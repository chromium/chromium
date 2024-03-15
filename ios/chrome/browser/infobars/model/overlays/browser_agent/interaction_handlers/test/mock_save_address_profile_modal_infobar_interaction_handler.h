// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_ADDRESS_PROFILE_MODAL_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_ADDRESS_PROFILE_MODAL_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#include "testing/gmock/include/gmock/gmock.h"

class AutofillProfile;
class InfoBarIOS;

// Mock version of SaveAddressProfileInfobarModalInteractionHandler for use in
// tests.
class MockSaveAddressProfileInfobarModalInteractionHandler
    : public SaveAddressProfileInfobarModalInteractionHandler {
 public:
  MockSaveAddressProfileInfobarModalInteractionHandler();
  ~MockSaveAddressProfileInfobarModalInteractionHandler() override;

  MOCK_METHOD2(SaveEditedProfile,
               void(InfoBarIOS* infobar, autofill::AutofillProfile* profile));
  MOCK_METHOD2(CancelModal, void(InfoBarIOS* infobar, BOOL fromEditView));
  MOCK_METHOD1(NoThanksWasPressed, void(InfoBarIOS* infobar));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_ADDRESS_PROFILE_MODAL_INFOBAR_INTERACTION_HANDLER_H_

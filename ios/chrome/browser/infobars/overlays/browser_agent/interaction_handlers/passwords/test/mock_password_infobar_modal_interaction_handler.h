// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"

#include "testing/gmock/include/gmock/gmock.h"

// Mock version of PasswordInfobarModalInteractionHandler for use in tests.
class MockPasswordInfobarModalInteractionHandler
    : public PasswordInfobarModalInteractionHandler {
 public:
  MockPasswordInfobarModalInteractionHandler();
  ~MockPasswordInfobarModalInteractionHandler();

  MOCK_METHOD3(UpdateCredentials,
               void(InfoBarIOS* infobar,
                    NSString* username,
                    NSString* password));
  MOCK_METHOD1(NeverSaveCredentials, void(InfoBarIOS* infobar));
  MOCK_METHOD1(PresentPasswordsSettings, void(InfoBarIOS* infobar));
  MOCK_METHOD1(PerformMainAction, void(InfoBarIOS* infobar));
  MOCK_METHOD2(InfobarVisibilityChanged,
               void(InfoBarIOS* infobar, bool visible));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_MODAL_INTERACTION_HANDLER_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"

#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"
#import "testing/gmock/include/gmock/gmock.h"

// Mock version of PasswordInfobarBannerInteractionHandler. Used for testing.
class MockPasswordInfobarBannerInteractionHandler
    : public PasswordInfobarBannerInteractionHandler {
 public:
  MockPasswordInfobarBannerInteractionHandler(
      Browser* browser,
      password_modal::PasswordAction action_type);
  ~MockPasswordInfobarBannerInteractionHandler() override;

  MOCK_METHOD1(MainButtonTapped, void(InfoBarIOS* infobar));
  MOCK_METHOD2(ShowModalButtonTapped,
               void(InfoBarIOS* infobar, web::WebState* web_state));
  MOCK_METHOD1(BannerDismissedByUser, void(InfoBarIOS* infobar));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_TEST_MOCK_PASSWORD_INFOBAR_BANNER_INTERACTION_HANDLER_H_

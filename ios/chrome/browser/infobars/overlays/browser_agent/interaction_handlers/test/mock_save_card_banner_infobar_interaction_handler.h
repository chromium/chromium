// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"

#include <string.h>

#include "testing/gmock/include/gmock/gmock.h"

class InfoBarIOS;

// Mock version of SaveCardInfobarBannerInteractionHandler for use in tests.
class MockSaveCardInfobarBannerInteractionHandler
    : public SaveCardInfobarBannerInteractionHandler {
 public:
  MockSaveCardInfobarBannerInteractionHandler();
  ~MockSaveCardInfobarBannerInteractionHandler() override;

  MOCK_METHOD4(SaveCredentials,
               void(InfoBarIOS* infobar,
                    std::u16string cardholder_name,
                    std::u16string expiration_date_month,
                    std::u16string expiration_date_year));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_SAVE_CARD_BANNER_INFOBAR_INTERACTION_HANDLER_H_

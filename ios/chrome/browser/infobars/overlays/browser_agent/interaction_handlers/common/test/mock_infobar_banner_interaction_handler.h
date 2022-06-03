// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  MockInfobarBannerInteractionHandler();
  ~MockInfobarBannerInteractionHandler();

  MOCK_METHOD2(BannerVisibilityChanged,
               void(InfoBarIOS* infobar, bool visible));
  MOCK_METHOD1(MainButtonTapped, void(InfoBarIOS* infobar));
  MOCK_METHOD2(ShowModalButtonTapped,
               void(InfoBarIOS* infobar, web::WebState* web_state));
  MOCK_METHOD1(BannerDismissedByUser, void(InfoBarIOS* infobar));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_BANNER_INTERACTION_HANDLER_H_

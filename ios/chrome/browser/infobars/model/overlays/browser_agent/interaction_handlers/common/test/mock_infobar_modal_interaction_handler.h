// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  MockInfobarModalInteractionHandler();
  ~MockInfobarModalInteractionHandler() override;

  MOCK_METHOD1(PerformMainAction, void(InfoBarIOS* infobar));
  MOCK_METHOD0(CreateModalInstaller,
               std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>());
  MOCK_METHOD2(InfobarVisibilityChanged,
               void(InfoBarIOS* infobar, bool visible));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_TEST_MOCK_INFOBAR_MODAL_INTERACTION_HANDLER_H_

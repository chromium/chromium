// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_READING_LIST_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_READING_LIST_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_modal_infobar_interaction_handler.h"

#include "testing/gmock/include/gmock/gmock.h"

class InfoBarIOS;
class Browser;

// Mock version of ReadingListInfobarModalInteractionHandler for use in tests.
class MockReadingListInfobarModalInteractionHandler
    : public ReadingListInfobarModalInteractionHandler {
 public:
  MockReadingListInfobarModalInteractionHandler(Browser* browser);
  ~MockReadingListInfobarModalInteractionHandler();

  MOCK_METHOD1(NeverAsk, void(InfoBarIOS* infobar));
  MOCK_METHOD1(PerformMainAction, void(InfoBarIOS* infobar));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_READING_LIST_INFOBAR_INTERACTION_HANDLER_H_

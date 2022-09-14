// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_TRANSLATE_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_TRANSLATE_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"

#include "testing/gmock/include/gmock/gmock.h"

// Mock version of TranslateInfobarModalInteractionHandler for use in tests.
class MockTranslateInfobarModalInteractionHandler
    : public TranslateInfobarModalInteractionHandler {
 public:
  MockTranslateInfobarModalInteractionHandler();
  ~MockTranslateInfobarModalInteractionHandler() override;

  MOCK_METHOD1(ToggleAlwaysTranslate, void(InfoBarIOS* infobar));
  MOCK_METHOD1(ToggleNeverTranslateLanguage, void(InfoBarIOS* infobar));
  MOCK_METHOD1(ToggleNeverTranslateSite, void(InfoBarIOS* infobar));
  MOCK_METHOD1(RevertTranslation, void(InfoBarIOS* infobar));
  MOCK_METHOD3(UpdateLanguages,
               void(InfoBarIOS* infobar,
                    int source_language_index,
                    int target_language_index));
  MOCK_METHOD1(PerformMainAction, void(InfoBarIOS* infobar));
  MOCK_METHOD2(InfobarVisibilityChanged,
               void(InfoBarIOS* infobar, bool visible));
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TEST_MOCK_TRANSLATE_INFOBAR_INTERACTION_HANDLER_H_

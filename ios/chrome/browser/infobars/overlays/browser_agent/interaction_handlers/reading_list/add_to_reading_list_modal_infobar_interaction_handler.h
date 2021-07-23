// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_MODAL_INFOBAR_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_MODAL_INFOBAR_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_interaction_handler.h"

class IOSAddToReadingListInfobarDelegate;
class Browser;

// Helper object that updates the model layer for interaction events with the
// Reading List infobar modal UI.
class ReadingListInfobarModalInteractionHandler
    : public InfobarModalInteractionHandler {
 public:
  ReadingListInfobarModalInteractionHandler(Browser* browser);
  ~ReadingListInfobarModalInteractionHandler() override;

  // Instructs the handler that the user has used |infobar|'s modal UI to
  // request that the Reading List banner never be shown.
  virtual void NeverAsk(InfoBarIOS* infobar);

  // InfobarModalInteractionHandler:
  void PerformMainAction(InfoBarIOS* infobar) override;

  // InfobarInteractionHandler::Handler:
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override {}

 private:
  // InfobarModalInteractionHandler:
  std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
  CreateModalInstaller() override;

  IOSAddToReadingListInfobarDelegate* GetDelegate(InfoBarIOS* infobar);

  Browser* browser_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_MODAL_INFOBAR_INTERACTION_HANDLER_H_

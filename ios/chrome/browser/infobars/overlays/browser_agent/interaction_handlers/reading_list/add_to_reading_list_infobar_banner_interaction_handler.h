// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

class Browser;
class IOSAddToReadingListInfobarDelegate;

// Helper object that updates the model layer for interaction events with the
// add to reading list infobar banner UI.
class AddToReadingListInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  AddToReadingListInfobarBannerInteractionHandler(Browser* browser);
  ~AddToReadingListInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override {}

 private:
  IOSAddToReadingListInfobarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_READING_LIST_ADD_TO_READING_LIST_INFOBAR_BANNER_INTERACTION_HANDLER_H_

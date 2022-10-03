// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SYNC_ERROR_SYNC_ERROR_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SYNC_ERROR_SYNC_ERROR_INFOBAR_BANNER_INTERACTION_HANDLER_H_

class ConfirmInfoBarDelegate;

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

// Helper object that updates the model layer for interaction events with the
// sync error infobar banner UI.
class SyncErrorInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  SyncErrorInfobarBannerInteractionHandler();
  ~SyncErrorInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

 private:
  // Returns the confirm infobar delegate from `infobar`.
  ConfirmInfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SYNC_ERROR_SYNC_ERROR_INFOBAR_BANNER_INTERACTION_HANDLER_H_

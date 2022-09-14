// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

#include "components/translate/core/browser/translate_infobar_delegate.h"

// Helper object that updates the model layer for interaction events with the
// Translate infobar banner UI.
class TranslateInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  TranslateInfobarBannerInteractionHandler();
  ~TranslateInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;

 private:
  // Returns the password delegate from `infobar`.
  translate::TranslateInfoBarDelegate* GetInfobarDelegate(InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_BANNER_INTERACTION_HANDLER_H_

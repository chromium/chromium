// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

// Default helper object that updates the model layer for interaction events
// with the infobar banner UI.
class DefaultInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  explicit DefaultInfobarBannerInteractionHandler(InfobarType infobar_type);
  ~DefaultInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void ShowModalButtonTapped(InfoBarIOS* infobar,
                             web::WebState* web_state) override;
  void BannerDismissedByUser(InfoBarIOS* infobar) override;

 private:
  const InfobarType infobar_type_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_DEFAULT_DEFAULT_INFOBAR_BANNER_INTERACTION_HANDLER_H_

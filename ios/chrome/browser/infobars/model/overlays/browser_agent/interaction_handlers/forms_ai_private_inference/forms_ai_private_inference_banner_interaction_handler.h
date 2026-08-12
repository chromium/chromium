// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_FORMS_AI_PRIVATE_INFERENCE_FORMS_AI_PRIVATE_INFERENCE_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_FORMS_AI_PRIVATE_INFERENCE_FORMS_AI_PRIVATE_INFERENCE_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

@class CommandDispatcher;

// Interaction handler for the Forms AI Private Inference notice banner.
class FormsAiPrivateInferenceBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  explicit FormsAiPrivateInferenceBannerInteractionHandler(
      CommandDispatcher* dispatcher);
  ~FormsAiPrivateInferenceBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;
  void ShowModalButtonTapped(InfoBarIOS* infobar,
                             web::WebState* web_state) override;
  void BannerDismissedByUser(InfoBarIOS* infobar) override;
  void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

 private:
  // The command dispatcher.
  __weak CommandDispatcher* dispatcher_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_FORMS_AI_PRIVATE_INFERENCE_FORMS_AI_PRIVATE_INFERENCE_BANNER_INTERACTION_HANDLER_H_

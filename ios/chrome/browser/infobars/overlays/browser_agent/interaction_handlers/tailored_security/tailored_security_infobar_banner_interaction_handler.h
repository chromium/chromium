// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TAILORED_SECURITY_TAILORED_SECURITY_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TAILORED_SECURITY_TAILORED_SECURITY_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_interaction_handler.h"

namespace safe_browsing {
class TailoredSecurityServiceInfobarDelegate;
}  // namespace safe_browsing

// Helper object that updates the model layer for interaction events with the
// tailored security infobar banner UI.
class TailoredSecurityInfobarBannerInteractionHandler
    : public InfobarBannerInteractionHandler {
 public:
  TailoredSecurityInfobarBannerInteractionHandler(
      const OverlayRequestSupport* request_support);
  ~TailoredSecurityInfobarBannerInteractionHandler() override;

  // InfobarBannerInteractionHandler:
  void MainButtonTapped(InfoBarIOS* infobar) override;

 private:
  // Returns the tailored security delegate from `infobar`.
  safe_browsing::TailoredSecurityServiceInfobarDelegate* GetInfobarDelegate(
      InfoBarIOS* infobar);
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TAILORED_SECURITY_TAILORED_SECURITY_INFOBAR_BANNER_INTERACTION_HANDLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

class OverlayRequestSupport;
class InfobarBannerOverlayRequestCallbackInstaller;

namespace web {
class WebState;
}

// A InfobarInteractionHandler::InteractionHandler, that handles interaction
// events for the high-level confirm infobar banner. This class can be
// subclassed to handle events differently for a different Infobar.
class InfobarBannerInteractionHandler
    : public InfobarInteractionHandler::Handler {
 public:
  // Constructor for a banner interaction handler that creates callback
  // installers with `request_support`.
  explicit InfobarBannerInteractionHandler(
      const OverlayRequestSupport* request_support);
  ~InfobarBannerInteractionHandler() override;

  // Updates the model when the visibility of `infobar`'s banner is changed.
  virtual void BannerVisibilityChanged(InfoBarIOS* infobar, bool visible) {}
  // Updates the model when the main button is tapped for `infobar`'s banner.
  virtual void MainButtonTapped(InfoBarIOS* infobar) {}
  // Shows the modal when the modal button is tapped for `infobar`'s banner.
  // `web_state` is the WebState associated with `infobar`'s InfoBarManager.
  virtual void ShowModalButtonTapped(InfoBarIOS* infobar,
                                     web::WebState* web_state);
  // Notifies the model that the upcoming dismissal is user-initiated (i.e.
  // swipe dismissal in the refresh UI).
  virtual void BannerDismissedByUser(InfoBarIOS* infobar);

 protected:
  // InfobarInteractionHandler::Handler:
  std::unique_ptr<OverlayRequestCallbackInstaller> CreateInstaller() override;
  void InfobarVisibilityChanged(InfoBarIOS* infobar, bool visible) override;

  // Creates the infobar banner callback installer for this handler.
  virtual std::unique_ptr<InfobarBannerOverlayRequestCallbackInstaller>
  CreateBannerInstaller();

  // The request support passed on initialization.  Only interactions with
  // supported requests should be handled by this instance.
  raw_ptr<const OverlayRequestSupport> request_support_ = nullptr;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_INTERACTION_HANDLER_H_

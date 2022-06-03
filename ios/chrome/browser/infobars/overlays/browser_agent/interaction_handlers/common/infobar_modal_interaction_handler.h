// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_INTERACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_INTERACTION_HANDLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

// A InfobarInteractionHandler::InteractionHandler, intended to be subclassed,
// that handles interaction events for an infobar modal.
class InfobarModalInteractionHandler
    : public InfobarInteractionHandler::Handler {
 public:
  ~InfobarModalInteractionHandler() override;

  // Updates the model to perform the main action for |infobar|.
  virtual void PerformMainAction(InfoBarIOS* infobar) = 0;

 protected:
  InfobarModalInteractionHandler();

  // Creates the infobar modal callback installer for this handler.
  virtual std::unique_ptr<InfobarModalOverlayRequestCallbackInstaller>
  CreateModalInstaller() = 0;

  // InfobarInteractionHandler::Handler:
  std::unique_ptr<OverlayRequestCallbackInstaller> CreateInstaller() final;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_INTERACTION_HANDLER_H_

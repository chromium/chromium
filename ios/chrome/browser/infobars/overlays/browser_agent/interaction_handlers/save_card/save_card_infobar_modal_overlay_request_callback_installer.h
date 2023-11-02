// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#include "base/memory/weak_ptr.h"

class SaveCardInfobarModalInteractionHandler;

// Callback installer for SaveCard infobar modal interaction events.
class SaveCardInfobarModalOverlayRequestCallbackInstaller
    : public InfobarModalOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler`.
  explicit SaveCardInfobarModalOverlayRequestCallbackInstaller(
      SaveCardInfobarModalInteractionHandler* interaction_handler);
  ~SaveCardInfobarModalOverlayRequestCallbackInstaller() override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // save_card_infobar_modal_responses::SaveCardMainAction.
  void SaveCardCredentialsCallback(OverlayRequest* request,
                                   OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // save_card_infobar_modal_responses::SaveCardLoadURL.
  void LoadURLCallback(OverlayRequest* request, OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for received responses.
  SaveCardInfobarModalInteractionHandler* interaction_handler_ = nullptr;

  base::WeakPtrFactory<SaveCardInfobarModalOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

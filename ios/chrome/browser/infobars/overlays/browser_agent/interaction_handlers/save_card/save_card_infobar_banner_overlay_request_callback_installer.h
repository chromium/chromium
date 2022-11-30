// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"

class SaveCardInfobarBannerInteractionHandler;

// Callback installer for SaveCard banner interaction events.
class SaveCardInfobarBannerOverlayRequestCallbackInstaller
    : public InfobarBannerOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler`.
  explicit SaveCardInfobarBannerOverlayRequestCallbackInstaller(
      SaveCardInfobarBannerInteractionHandler* interaction_handler);
  ~SaveCardInfobarBannerOverlayRequestCallbackInstaller() override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // save_card_infobar_modal_responses::SaveCardMainAction.
  void SaveCredentialsCallback(OverlayRequest* request,
                               OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for received responses.
  SaveCardInfobarBannerInteractionHandler* interaction_handler_ = nullptr;

  base::WeakPtrFactory<SaveCardInfobarBannerOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_SAVE_CARD_SAVE_CARD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

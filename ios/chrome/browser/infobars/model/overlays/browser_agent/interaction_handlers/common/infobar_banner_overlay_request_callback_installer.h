// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/overlays/model/public/overlay_request_callback_installer.h"

class OverlayRequestSupport;
class InfobarBannerInteractionHandler;

// Installer for callbacks that are added to OverlayRequests for infobar
// banners.
class InfobarBannerOverlayRequestCallbackInstaller
    : public OverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks for OverlayRequests
  // supported by `request_support` that forward interaction events to
  // `interaction_handler`.
  InfobarBannerOverlayRequestCallbackInstaller(
      const OverlayRequestSupport* request_support,
      InfobarBannerInteractionHandler* interaction_handler);
  ~InfobarBannerOverlayRequestCallbackInstaller() override;

 protected:
  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // InfobarBannerMainActionResponse.
  void MainActionButtonTapped(OverlayRequest* request,
                              OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // InfobarBannerShowModalResponse.
  void ShowModalButtonTapped(OverlayRequest* request,
                             OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // InfobarBannerUserInitiatedDismissalResponse.
  void BannerDismissedByUser(OverlayRequest* request,
                             OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // InfobarBannerRemoveInfobarResponse.
  void RemoveInfobar(OverlayRequest* request, OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  const OverlayRequestSupport* GetRequestSupport() const override;

  // The request support for `interaction_handler_`.
  raw_ptr<const OverlayRequestSupport> request_support_ = nullptr;
  // The handler for received responses.
  raw_ptr<InfobarBannerInteractionHandler> interaction_handler_ = nullptr;

  base::WeakPtrFactory<InfobarBannerOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

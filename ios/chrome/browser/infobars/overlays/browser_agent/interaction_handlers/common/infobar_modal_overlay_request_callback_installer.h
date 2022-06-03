// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/overlays/public/overlay_request_callback_installer.h"

class InfobarModalInteractionHandler;

// Callback installer, intended to be subclassed, for infobar modal interaction
// events.
class InfobarModalOverlayRequestCallbackInstaller
    : public OverlayRequestCallbackInstaller {
 public:
  ~InfobarModalOverlayRequestCallbackInstaller() override;

 protected:
  // Constructor for an instance that installs callbacks for OverlayRequests
  // supported by |request_support| that forward interaction events to
  // |interaction_handler|.
  InfobarModalOverlayRequestCallbackInstaller(
      const OverlayRequestSupport* request_support,
      InfobarModalInteractionHandler* interaction_handler);

  // OverlayRequestCallbackInstaller override.  Subclasses should call
  // InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal()
  // so that the modal handler's main action can be triggered.
  void InstallCallbacksInternal(OverlayRequest* request) override;

 private:
  // Used as a callback for OverlayResponses dispatched through |request|'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that |response| is created with an
  // InfobarModalMainActionResponse.
  void MainActionCallback(OverlayRequest* request, OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  const OverlayRequestSupport* GetRequestSupport() const override;

  // The request support for |interaction_handler_|.
  const OverlayRequestSupport* request_support_ = nullptr;
  // The handler for received responses.
  InfobarModalInteractionHandler* interaction_handler_ = nullptr;

  base::WeakPtrFactory<InfobarModalOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_COMMON_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

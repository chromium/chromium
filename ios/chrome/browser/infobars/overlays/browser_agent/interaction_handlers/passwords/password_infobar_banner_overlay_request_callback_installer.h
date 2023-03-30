// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_banner_overlay_request_callback_installer.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"

class PasswordInfobarBannerInteractionHandler;

// Callback installer for password infobar banners.
class PasswordInfobarBannerOverlayRequestCallbackInstaller
    : public InfobarBannerOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler` for an Password Infobar Overlay
  // of type `action_type`.
  explicit PasswordInfobarBannerOverlayRequestCallbackInstaller(
      PasswordInfobarBannerInteractionHandler* interaction_handler,
      password_modal::PasswordAction action_type);
  ~PasswordInfobarBannerOverlayRequestCallbackInstaller() override;

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
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for receiving responses.
  PasswordInfobarBannerInteractionHandler* interaction_handler_ = nullptr;

  // The type of Password Infobar Overlay this handler is managing.
  password_modal::PasswordAction password_action_;

  base::WeakPtrFactory<PasswordInfobarBannerOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_BANNER_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

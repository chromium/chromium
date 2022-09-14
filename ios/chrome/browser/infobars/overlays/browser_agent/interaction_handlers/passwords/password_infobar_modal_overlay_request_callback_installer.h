// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#include "base/memory/weak_ptr.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/password_infobar_modal_overlay_request_config.h"

class PasswordInfobarModalInteractionHandler;

// Callback installer, intended to be subclassed, for infobar modal interaction
// events.
class PasswordInfobarModalOverlayRequestCallbackInstaller
    : public InfobarModalOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler` for an Password Infobar Overlay
  // of type `action_type`.
  explicit PasswordInfobarModalOverlayRequestCallbackInstaller(
      PasswordInfobarModalInteractionHandler* interaction_handler,
      password_modal::PasswordAction action_type);
  ~PasswordInfobarModalOverlayRequestCallbackInstaller() override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // password_infobar_modal_responses::UpdateCredentialsInfo.
  void UpdateCredentialsCallback(OverlayRequest* request,
                                 OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // password_infobar_modal_responses::NeverSaveCredentials.
  void NeverSaveCredentialsCallback(OverlayRequest* request,
                                    OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // password_infobar_modal_responses::PresentPasswordSettings.
  void PresentPasswordsSettingsCallback(OverlayRequest* request,
                                        OverlayResponse* response);

  // Used as an optional completion callback for `request`.  Removes the
  // request's infobar from its manager upon completion.
  void RemoveInfobarCompletionCallback(OverlayRequest* request,
                                       OverlayResponse* response);
  // Used as an optional completion callback for `request`.  Presents the
  // password settings.
  void PresentPasswordSettingsCompletionCallback(OverlayRequest* request,
                                                 OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for received responses.
  PasswordInfobarModalInteractionHandler* interaction_handler_ = nullptr;

  // The type of Password Infobar Overlay this installer handles.
  password_modal::PasswordAction password_action_;

  base::WeakPtrFactory<PasswordInfobarModalOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_PASSWORDS_PASSWORD_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

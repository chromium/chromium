// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

class SaveAddressProfileInfobarModalInteractionHandler;

// Callback installer for SaveAddressProfile infobar modal interaction events.
class SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller
    : public InfobarModalOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler`.
  explicit SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller(
      SaveAddressProfileInfobarModalInteractionHandler* interaction_handler);
  ~SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller() override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // save_address_profile_infobar_modal_responses::EditedProfileSaveAction.
  void SaveEditedProfileDetailsCallback(OverlayRequest* request,
                                        OverlayResponse* response);

  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // save_address_profile_infobar_modal_responses::NoThanksViewAction.
  void NoThanksCallback(OverlayRequest* request, OverlayResponse* response);

  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager.  The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // save_address_profile_infobar_modal_responses::CancelViewAction.
  void CancelModalCallback(OverlayRequest* request, OverlayResponse* response);

  // Used as an optional completion callback for `request`.  Removes the
  // request's infobar from its manager upon completion.
  void RemoveInfobarCompletionCallback(OverlayRequest* request,
                                       OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for received responses.
  raw_ptr<SaveAddressProfileInfobarModalInteractionHandler>
      interaction_handler_ = nullptr;

  base::WeakPtrFactory<
      SaveAddressProfileInfobarModalOverlayRequestCallbackInstaller>
      weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_AUTOFILL_ADDRESS_PROFILE_SAVE_ADDRESS_PROFILE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/common/infobar_modal_overlay_request_callback_installer.h"

#include "base/memory/weak_ptr.h"

class TranslateInfobarModalInteractionHandler;

namespace translate_infobar_overlay {

// Callback installer, intended to be subclassed, for infobar modal interaction
// events.
class ModalRequestCallbackInstaller
    : public InfobarModalOverlayRequestCallbackInstaller {
 public:
  // Constructor for an instance that installs callbacks that forward
  // interaction events to `interaction_handler`.
  explicit ModalRequestCallbackInstaller(
      TranslateInfobarModalInteractionHandler* interaction_handler);
  ~ModalRequestCallbackInstaller() override;

 private:
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager. The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // translate_infobar_modal_responses::RevertMainAction.
  void RevertTranslationCallback(OverlayRequest* request,
                                 OverlayResponse* response);

  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager. The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with an
  // translate_infobar_modal_responses::ToggleAlwaysTranslate.
  void ToggleAlwaysTranslateCallback(OverlayRequest* request,
                                     OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager. The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // translate_infobar_modal_responses::ToggleNeverTranslateSourceLanguage.
  void ToggleNeverTranslateSourceLanguageCallback(OverlayRequest* request,
                                                  OverlayResponse* response);
  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager. The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // translate_infobar_modal_responses::ToggleNeverPromptSite.
  void ToggleNeverTranslateSiteCallback(OverlayRequest* request,
                                        OverlayResponse* response);

  // Used as a callback for OverlayResponses dispatched through `request`'s
  // callback manager. The OverlayDispatchCallback is created with an
  // OverlayResponseSupport that guarantees that `response` is created with a
  // translate_infobar_modal_responses::UpdateLanguageInfo.
  void UpdateLanguageCallback(OverlayRequest* request,
                              OverlayResponse* response);

  // OverlayRequestCallbackInstaller:
  void InstallCallbacksInternal(OverlayRequest* request) override;

  // The handler for received responses.
  TranslateInfobarModalInteractionHandler* interaction_handler_ = nullptr;

  base::WeakPtrFactory<ModalRequestCallbackInstaller> weak_factory_{this};
};

}  // namespace translate_infobar_overlay

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INTERACTION_HANDLERS_TRANSLATE_TRANSLATE_INFOBAR_MODAL_OVERLAY_REQUEST_CALLBACK_INSTALLER_H_

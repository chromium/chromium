// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_overlay_request_callback_installer.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_request_config.h"
#import "ios/chrome/browser/overlays/public/infobar_modal/translate_infobar_modal_overlay_responses.h"
#import "ios/chrome/browser/overlays/public/overlay_callback_manager.h"
#import "ios/chrome/browser/overlays/public/overlay_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using translate_infobar_overlays::TranslateModalRequestConfig;
using translate_infobar_modal_responses::RevertTranslation;
using translate_infobar_modal_responses::ToggleAlwaysTranslate;
using translate_infobar_modal_responses::ToggleNeverTranslateSourceLanguage;
using translate_infobar_modal_responses::ToggleNeverPromptSite;
using translate_infobar_modal_responses::UpdateLanguageInfo;

namespace translate_infobar_overlay {

ModalRequestCallbackInstaller::ModalRequestCallbackInstaller(
    TranslateInfobarModalInteractionHandler* interaction_handler)
    : InfobarModalOverlayRequestCallbackInstaller(
          TranslateModalRequestConfig::RequestSupport(),
          interaction_handler),
      interaction_handler_(interaction_handler) {
  DCHECK(interaction_handler_);
}

ModalRequestCallbackInstaller::~ModalRequestCallbackInstaller() = default;

#pragma mark - Private

void ModalRequestCallbackInstaller::RevertTranslationCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;

  interaction_handler_->RevertTranslation(infobar);
}

void ModalRequestCallbackInstaller::ToggleAlwaysTranslateCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;

  interaction_handler_->ToggleAlwaysTranslate(infobar);
}

void ModalRequestCallbackInstaller::ToggleNeverTranslateSourceLanguageCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->ToggleNeverTranslateLanguage(infobar);
}

void ModalRequestCallbackInstaller::ToggleNeverTranslateSiteCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  interaction_handler_->ToggleNeverTranslateSite(infobar);
}

void ModalRequestCallbackInstaller::UpdateLanguageCallback(
    OverlayRequest* request,
    OverlayResponse* response) {
  InfoBarIOS* infobar = GetOverlayRequestInfobar(request);
  if (!infobar)
    return;
  UpdateLanguageInfo* response_info = response->GetInfo<UpdateLanguageInfo>();

  interaction_handler_->UpdateLanguages(infobar,
                                        response_info->source_language_index(),
                                        response_info->target_language_index());
}

#pragma mark - OverlayRequestCallbackInstaller

void ModalRequestCallbackInstaller::InstallCallbacksInternal(
    OverlayRequest* request) {
  InfobarModalOverlayRequestCallbackInstaller::InstallCallbacksInternal(
      request);
  OverlayCallbackManager* manager = request->GetCallbackManager();

  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &ModalRequestCallbackInstaller::RevertTranslationCallback,
          weak_factory_.GetWeakPtr(), request),
      RevertTranslation::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &ModalRequestCallbackInstaller::ToggleAlwaysTranslateCallback,
          weak_factory_.GetWeakPtr(), request),
      ToggleAlwaysTranslate::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(&ModalRequestCallbackInstaller::
                              ToggleNeverTranslateSourceLanguageCallback,
                          weak_factory_.GetWeakPtr(), request),
      ToggleNeverTranslateSourceLanguage::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &ModalRequestCallbackInstaller::ToggleNeverTranslateSiteCallback,
          weak_factory_.GetWeakPtr(), request),
      ToggleNeverPromptSite::ResponseSupport()));
  manager->AddDispatchCallback(OverlayDispatchCallback(
      base::BindRepeating(
          &ModalRequestCallbackInstaller::UpdateLanguageCallback,
          weak_factory_.GetWeakPtr(), request),
      UpdateLanguageInfo::ResponseSupport()));
}

}  // namespace translate_infobar_overlay

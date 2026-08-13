// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/forms_ai_private_inference/forms_ai_private_inference_banner_interaction_handler.h"

#import "base/check.h"
#import "components/infobars/core/confirm_infobar_delegate.h"
#import "ios/chrome/browser/autofill/model/forms_ai_private_inference_infobar_delegate_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/overlays/model/public/default/default_infobar_overlay_request_config.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"

#pragma mark - FormsAiPrivateInferenceBannerInteractionHandler

FormsAiPrivateInferenceBannerInteractionHandler::
    FormsAiPrivateInferenceBannerInteractionHandler(
        CommandDispatcher* dispatcher)
    : InfobarBannerInteractionHandler(
          DefaultInfobarOverlayRequestConfig::RequestSupport()),
      dispatcher_(dispatcher) {}

FormsAiPrivateInferenceBannerInteractionHandler::
    ~FormsAiPrivateInferenceBannerInteractionHandler() = default;

void FormsAiPrivateInferenceBannerInteractionHandler::MainButtonTapped(
    InfoBarIOS* infobar) {
  if (infobar->infobar_type() !=
      InfobarType::kInfobarTypeFormsAiPrivateInference) {
    return;
  }
  ConfirmInfoBarDelegate* delegate =
      infobar->delegate()->AsConfirmInfoBarDelegate();
  DCHECK(delegate);
  delegate->Accept();
}

void FormsAiPrivateInferenceBannerInteractionHandler::ShowModalButtonTapped(
    InfoBarIOS* infobar,
    web::WebState* web_state) {
  if (infobar->infobar_type() !=
      InfobarType::kInfobarTypeFormsAiPrivateInference) {
    return;
  }
  auto* private_inference_delegate =
      static_cast<FormsAiPrivateInferenceInfoBarDelegateIOS*>(
          infobar->delegate()->AsConfirmInfoBarDelegate());
  DCHECK(private_inference_delegate);
  private_inference_delegate->OnSettingsLinkClicked();

  id<SettingsCommands> settings_commands_handler =
      HandlerForProtocol(dispatcher_, SettingsCommands);
  [settings_commands_handler showAutofillSettingsFromNotice];
  InfobarBannerInteractionHandler::BannerDismissedByUser(infobar);
}

void FormsAiPrivateInferenceBannerInteractionHandler::BannerDismissedByUser(
    InfoBarIOS* infobar) {
  if (infobar->infobar_type() !=
      InfobarType::kInfobarTypeFormsAiPrivateInference) {
    return;
  }
  InfobarBannerInteractionHandler::BannerDismissedByUser(infobar);
}

void FormsAiPrivateInferenceBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  if (infobar->infobar_type() !=
      InfobarType::kInfobarTypeFormsAiPrivateInference) {
    return;
  }
  if (!visible) {
    ConfirmInfoBarDelegate* delegate =
        infobar->delegate()->AsConfirmInfoBarDelegate();
    DCHECK(delegate);
    delegate->InfoBarDismissed();
  }
  InfobarBannerInteractionHandler::BannerVisibilityChanged(infobar, visible);
}

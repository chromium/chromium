// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"

#import "base/feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/confirm/confirm_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/permissions/permissions_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/sync_error/sync_error_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/password_infobar_banner_overlay.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/tailored_security_service_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachInfobarOverlayBrowserAgent(Browser* browser) {
  InfobarOverlayBrowserAgent::CreateForBrowser(browser);
  InfobarOverlayBrowserAgent* browser_agent =
      InfobarOverlayBrowserAgent::FromBrowser(browser);
  {
    auto banner_handler =
        std::make_unique<PasswordInfobarBannerInteractionHandler>(
            browser,
            PasswordInfobarBannerOverlayRequestConfig::RequestSupport());
    auto modal_handler =
        std::make_unique<PasswordInfobarModalInteractionHandler>(
            browser, password_modal::PasswordAction::kSave);
    browser_agent->AddInfobarInteractionHandler(
        std::make_unique<InfobarInteractionHandler>(
            InfobarType::kInfobarTypePasswordSave, std::move(banner_handler),
            std::move(modal_handler)));
  }
  {
    auto banner_handler =
        std::make_unique<PasswordInfobarBannerInteractionHandler>(
            browser,
            PasswordInfobarBannerOverlayRequestConfig::RequestSupport());
    auto modal_handler =
        std::make_unique<PasswordInfobarModalInteractionHandler>(
            browser, password_modal::PasswordAction::kUpdate);
    browser_agent->AddInfobarInteractionHandler(
        std::make_unique<InfobarInteractionHandler>(
            InfobarType::kInfobarTypePasswordUpdate, std::move(banner_handler),
            std::move(modal_handler)));
  }
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypeConfirm,
          std::make_unique<ConfirmInfobarBannerInteractionHandler>(),
          /*modal_handler=*/nullptr));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypeTranslate,
          std::make_unique<TranslateInfobarBannerInteractionHandler>(),
          std::make_unique<TranslateInfobarModalInteractionHandler>()));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypeSaveCard,
          std::make_unique<SaveCardInfobarBannerInteractionHandler>(),
          std::make_unique<SaveCardInfobarModalInteractionHandler>()));
  browser_agent->AddInfobarInteractionHandler(std::make_unique<
                                              InfobarInteractionHandler>(
      InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      std::make_unique<SaveAddressProfileInfobarBannerInteractionHandler>(),
      std::make_unique<SaveAddressProfileInfobarModalInteractionHandler>()));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypePermissions,
          std::make_unique<PermissionsInfobarBannerInteractionHandler>(),
          /*modal_handler=*/nullptr));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypeSyncError,
          std::make_unique<SyncErrorInfobarBannerInteractionHandler>(),
          /*modal_handler=*/nullptr));

  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    const OverlayRequestSupport* support =
        tailored_security_service_infobar_overlays::
            TailoredSecurityServiceBannerRequestConfig::RequestSupport();
    browser_agent->AddInfobarInteractionHandler(
        std::make_unique<InfobarInteractionHandler>(
            InfobarType::kInfobarTypeTailoredSecurityService,
            std::make_unique<TailoredSecurityInfobarBannerInteractionHandler>(
                support),
            nil));
  }
}

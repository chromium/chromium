// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent_util.h"

#import "base/feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/infobar_overlay_browser_agent.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/confirm/confirm_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/password_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/passwords/update_password_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/permissions/permissions_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/tailored_security/tailored_security_infobar_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_interaction_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void AttachInfobarOverlayBrowserAgent(Browser* browser) {
  InfobarOverlayBrowserAgent::CreateForBrowser(browser);
  InfobarOverlayBrowserAgent* browser_agent =
      InfobarOverlayBrowserAgent::FromBrowser(browser);
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<PasswordInfobarInteractionHandler>(browser));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<UpdatePasswordInfobarInteractionHandler>(browser));
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<ConfirmInfobarInteractionHandler>());
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<TranslateInfobarInteractionHandler>());
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<SaveCardInfobarInteractionHandler>());
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<SaveAddressProfileInfobarInteractionHandler>());
  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<PermissionsInfobarInteractionHandler>());
  if (base::FeatureList::IsEnabled(
          safe_browsing::kTailoredSecurityIntegration)) {
    browser_agent->AddInfobarInteractionHandler(
        std::make_unique<TailoredSecurityInfobarInteractionHandler>());
  }
}

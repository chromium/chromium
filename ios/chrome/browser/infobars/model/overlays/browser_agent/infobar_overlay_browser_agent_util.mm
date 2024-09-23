// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/overlays/browser_agent/infobar_overlay_browser_agent_util.h"

#import "base/feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/infobar_overlay_browser_agent.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/model/overlays/browser_agent/interaction_handlers/confirm/confirm_infobar_banner_interaction_handler.h"

void AttachInfobarOverlayBrowserAgent(Browser* browser) {
  InfobarOverlayBrowserAgent::CreateForBrowser(browser);
  InfobarOverlayBrowserAgent* browser_agent =
      InfobarOverlayBrowserAgent::FromBrowser(browser);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeTailoredSecurityService);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypePasswordSave);
  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypePasswordUpdate);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypePermissions);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeSaveCard);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeSyncError);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeTranslate);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeParcelTracking);

  browser_agent->AddDefaultInfobarInteractionHandlerForInfobarType(
      InfobarType::kInfobarTypeEnhancedSafeBrowsing);

  browser_agent->AddInfobarInteractionHandler(
      std::make_unique<InfobarInteractionHandler>(
          InfobarType::kInfobarTypeConfirm,
          std::make_unique<ConfirmInfobarBannerInteractionHandler>(),
          /*modal_handler=*/nullptr));
  browser_agent->AddInfobarInteractionHandler(std::make_unique<
                                              InfobarInteractionHandler>(
      InfobarType::kInfobarTypeSaveAutofillAddressProfile,
      std::make_unique<SaveAddressProfileInfobarBannerInteractionHandler>(),
      std::make_unique<SaveAddressProfileInfobarModalInteractionHandler>()));
}

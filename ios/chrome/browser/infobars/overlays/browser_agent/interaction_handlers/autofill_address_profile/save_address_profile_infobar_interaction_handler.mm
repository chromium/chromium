// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_modal_interaction_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SaveAddressProfileInfobarInteractionHandler::
    SaveAddressProfileInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypeSaveAutofillAddressProfile,
          std::make_unique<SaveAddressProfileInfobarBannerInteractionHandler>(),
          std::make_unique<
              SaveAddressProfileInfobarModalInteractionHandler>()) {}

SaveAddressProfileInfobarInteractionHandler::
    ~SaveAddressProfileInfobarInteractionHandler() = default;

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/save_card/save_card_infobar_modal_interaction_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

SaveCardInfobarInteractionHandler::SaveCardInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypeSaveCard,
          std::make_unique<SaveCardInfobarBannerInteractionHandler>(),
          std::make_unique<SaveCardInfobarModalInteractionHandler>()) {}

SaveCardInfobarInteractionHandler::~SaveCardInfobarInteractionHandler() =
    default;

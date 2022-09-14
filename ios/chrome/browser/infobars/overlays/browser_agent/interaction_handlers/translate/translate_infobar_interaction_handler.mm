// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_interaction_handler.h"

#import "components/infobars/core/infobar.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/translate/translate_infobar_modal_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/main/browser.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TranslateInfobarInteractionHandler::TranslateInfobarInteractionHandler()
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypeTranslate,
          std::make_unique<TranslateInfobarBannerInteractionHandler>(),
          std::make_unique<TranslateInfobarModalInteractionHandler>()) {}

TranslateInfobarInteractionHandler::~TranslateInfobarInteractionHandler() =
    default;

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_interaction_handler.h"

#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_infobar_banner_interaction_handler.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/reading_list/add_to_reading_list_modal_infobar_interaction_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

AddToReadingListInfobarInteractionHandler::
    AddToReadingListInfobarInteractionHandler(Browser* browser)
    : InfobarInteractionHandler(
          InfobarType::kInfobarTypeAddToReadingList,
          std::make_unique<AddToReadingListInfobarBannerInteractionHandler>(
              browser),
          std::make_unique<ReadingListInfobarModalInteractionHandler>(
              browser)) {}

AddToReadingListInfobarInteractionHandler::
    ~AddToReadingListInfobarInteractionHandler() = default;
